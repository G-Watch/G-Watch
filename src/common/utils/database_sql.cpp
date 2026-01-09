#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <mutex>
#include <filesystem>

#include <unistd.h>

#include "sqlite3.h"

#include "common/common.hpp"
#include "common/log.hpp"
#include "common/utils/timer.hpp"
#include "common/utils/exception.hpp"
#include "common/utils/database_sql.hpp"

#include <nlohmann/json.hpp>


GWUtilSQLDatabase::GWUtilSQLDatabase(const std::string directory) : GWUtilDatabase(){
    uint64_t tsc;
    int sdk_retval;
    pid_t pid;

    // get current TSC as the filename
    tsc = GWUtilTscTimer::get_tsc();
    pid = getpid();
    this->_filename = directory + "/gwatch_scheduler_sqldb_" + std::to_string(pid) + "_" + std::to_string(tsc) + ".sqlite";

    GW_IF_SQLITE_FAILED(
        sqlite3_open(this->_filename.c_str(), &(this->_db)),
        sdk_retval,
        {
            throw GWException(
                "failed to open sqlite database: file(%s), error(%s)",
                this->_filename.c_str(), std::string(sqlite3_errmsg(this->_db)).c_str()
            );
        }
    );
    GW_DEBUG_C("open sqlite database: file(%s)", this->_filename.c_str());
}


GWUtilSQLDatabase::~GWUtilSQLDatabase() {
    int sdk_retval;

    if(unlikely(this->_db)){
        GW_IF_SQLITE_FAILED(
            sqlite3_close(this->_db),
            sdk_retval,
            {
                GW_WARN_C(
                    "failed to close sqlite database: file(%s), error(%s)",
                    this->_filename.c_str(), std::string(sqlite3_errmsg(this->_db)).c_str())
                ;
            }
        );

        if(!sdk_retval){
            std::filesystem::remove(this->_filename);
            GW_DEBUG_C("close sqlite database: file(%s)", this->_filename.c_str());
        }
    }
}


gw_retval_t GWUtilSQLDatabase::create_table(const std::string &table_name, const std::string &schema){
    gw_retval_t retval = GW_SUCCESS;
    std::lock_guard<std::mutex> lock_callback(this->_callback_mutex);
    std::lock_guard<std::mutex> lock_db(this->_db_mutex);
    
    retval = this->__execute("CREATE TABLE IF NOT EXISTS " + table_name + " " + schema + ";");
    if(likely(retval == GW_SUCCESS)){
        GW_DEBUG_C("create table in SQL database: table_name(%s), schema(%s)", table_name.c_str(), schema.c_str());
    } else {
        GW_WARN_C("failed to create table in SQL database: table_name(%s), schema(%s)", table_name.c_str(), schema.c_str());
    }

exit:
    return retval;
}


gw_retval_t GWUtilSQLDatabase::drop_table(const std::string &table_name){
    gw_retval_t retval = GW_SUCCESS;
    std::lock_guard<std::mutex> lock_callback(this->_callback_mutex);
    std::lock_guard<std::mutex> lock_db(this->_db_mutex);
    
    retval = this->__execute("DROP TABLE IF EXISTS " + table_name + ";");
    if(likely(retval == GW_SUCCESS)){
        GW_DEBUG_C("drop table in SQL database: table_name(%s)", table_name.c_str());
    } else {
        GW_WARN_C("failed to drop table in SQL database: table_name(%s)", table_name.c_str());
    }

exit:
    return retval;
}


gw_retval_t GWUtilSQLDatabase::insert_row(const std::string &table_name, const std::map<std::string, nlohmann::json> &data){
    gw_retval_t retval = GW_SUCCESS;
    std::string cols, vals;
    std::string sql;
    GWUtilSqlQueryResult result;
    bool is_result_generated = false;
    std::vector<gw_db_subscribe_context_t*> list_subs_ctx = {};
    std::lock_guard<std::mutex> lock_callback(this->_callback_mutex);
    std::lock_guard<std::mutex> lock_db(this->_db_mutex);

    // escape single quotes in input to prevent SQL errors
    auto __escape_string = [](const std::string &input) {
        std::string output;
        output.reserve(input.size());
        for (char c : input) {
            if (c == '\'') output += "''";
            else output += c;
        }
        return output;
    };

    if(unlikely(data.empty())){ goto exit; }
    for (auto it = data.begin(); it != data.end(); it++) {
        cols += it->first + ",";
        vals += "'" + __escape_string(it->second.dump()) + "',";
    }

    // remove trailing comma
    cols.pop_back();
    vals.pop_back();

    sql = "INSERT INTO " + table_name + " (" + cols + ") VALUES (" + vals + ");";

    retval = this->__execute(sql);
    if(likely(retval == GW_SUCCESS)){
        GW_DEBUG_C("insert row in SQL database: table_name(%s), cols(%s), vals(%s)", table_name.c_str(), cols.c_str(), vals.c_str());
        
        // execute callback once the insertion is success
        this->__get_all_subscribers(nullptr, "", table_name, /* uri_precise_match */ true, list_subs_ctx);
        for(auto& subs_ctx : list_subs_ctx){
            if(unlikely(!is_result_generated)){
                // TODO(zhuobin): we must optimize SQL's streaming performance (incremental streaming?)
                sql = "SELECT * FROM " + table_name + ";";
                result.clear();
                GW_IF_FAILED(
                    this->query_lockless(sql, result),
                    retval,
                    {
                        GW_WARN_C(
                            "failed to query all result from dababase before executing callback: table_name(%s), sql(%s)",
                            table_name.c_str(), sql.c_str()
                        );
                        goto exit;
                    }
                );
                is_result_generated = true;
            }

            GW_IF_FAILED(
                subs_ctx->callback(
                    /* subscribe_cxt */ subs_ctx,
                    /* new_value */ result.serialize(),
                    /* old_value */ ""
                ),
                retval,
                {
                    GW_WARN_C("failed to execute callback, set failed: table_name(%s)", table_name.c_str());
                    goto exit;
                }
            );
        }
    } else {
        GW_WARN_C("failed to insert row in SQL database: table_name(%s), cols(%s), vals(%s)", table_name.c_str(), cols.c_str(), vals.c_str());
    }

exit:
    return retval;

}


gw_retval_t GWUtilSQLDatabase::delete_row(const std::string &table_name, const std::string &where_clause){
    gw_retval_t retval = GW_SUCCESS;
    std::string sql;
    GWUtilSqlQueryResult result;
    typename std::map<std::string, gw_db_subscribe_callback_t>::iterator cb_iter;
    bool is_result_generated = false;
    std::vector<gw_db_subscribe_context_t*> list_subs_ctx = {};
    std::lock_guard<std::mutex> lock_callback(this->_callback_mutex);
    std::lock_guard<std::mutex> lock_db(this->_db_mutex);

    sql = "DELETE FROM " + table_name;
    if (!where_clause.empty()) {
        sql += " WHERE " + where_clause;
    }
    sql += ";";

    retval = this->__execute(sql);
    if(likely(retval == GW_SUCCESS)){
        GW_DEBUG_C("delete row in SQL database: table_name(%s), where_clause(%s)", table_name.c_str(), where_clause.c_str());

        // execute callback once the deletion is success
        this->__get_all_subscribers(nullptr, "", table_name, /* uri_precise_match */ true, list_subs_ctx);
        for(auto& subs_ctx : list_subs_ctx){
            if(unlikely(!is_result_generated)){
                sql = "SELECT * FROM " + table_name + ";";
                result.clear();
                GW_IF_FAILED(
                    this->query_lockless(sql, result),
                    retval,
                    {
                        GW_WARN_C(
                            "failed to query all result from dababase before executing callback: table_name(%s), sql(%s)",
                            table_name.c_str(), sql.c_str()
                        );
                        goto exit;
                    }
                );
                is_result_generated = true;
            }

            GW_IF_FAILED(
                subs_ctx->callback(
                    /* subscribe_cxt */ subs_ctx,
                    /* new_value */ result.serialize(),
                    /* old_value */ ""
                ),
                retval,
                {
                    GW_WARN_C("failed to execute callback, set failed: table_name(%s)", table_name.c_str());
                    goto exit;
                }
            );
        }
    } else {
        GW_WARN_C("failed to delete row in SQL database: table_name(%s), where_clause(%s)", table_name.c_str(), where_clause.c_str());
    }

exit:
    return retval;
}


gw_retval_t GWUtilSQLDatabase::update_row(
    const std::string &table_name,
    const std::map<std::string, nlohmann::json> &data,
    const std::string &where_clause
){
    gw_retval_t retval = GW_SUCCESS;
    std::string sets;
    std::string sql;
    GWUtilSqlQueryResult result;
    bool is_result_generated = false;
    std::vector<gw_db_subscribe_context_t*> list_subs_ctx = {};
    std::lock_guard<std::mutex> lock_callback(this->_callback_mutex);
    std::lock_guard<std::mutex> lock_db(this->_db_mutex);

    // escape single quotes in input to prevent SQL errors
    auto __escape_string = [](const std::string &input) {
        std::string output;
        output.reserve(input.size());
        for (char c : input) {
            if (c == '\'') output += "''";
            else output += c;
        }
        return output;
    };

    if (unlikely(data.empty())) { goto exit; }

    for (auto it = data.begin(); it != data.end(); ++it) {
        sets += it->first + "='" + __escape_string(it->second.dump()) + "',";
    }

    // remove trailing comma
    if (!sets.empty()) sets.pop_back();

    sql = "UPDATE " + table_name + " SET " + sets;
    if (!where_clause.empty()) {
        sql += " WHERE " + where_clause;
    }
    sql += ";";

    retval = this->__execute(sql);
    if (likely(retval == GW_SUCCESS)) {
        GW_DEBUG_C("update row in SQL database: table_name(%s), sets(%s), where_clause(%s)",
                   table_name.c_str(), sets.c_str(), where_clause.c_str());

        // execute callback once the update is success
        this->__get_all_subscribers(nullptr, "", table_name, /* uri_precise_match */ true, list_subs_ctx);
        for (auto& subs_ctx : list_subs_ctx) {
            if (unlikely(!is_result_generated)) {
                sql = "SELECT * FROM " + table_name + ";";
                result.clear();
                GW_IF_FAILED(
                    this->query_lockless(sql, result),
                    retval,
                    {
                        GW_WARN_C(
                            "failed to query all result from dababase before executing callback: table_name(%s), sql(%s)",
                            table_name.c_str(), sql.c_str()
                        );
                        goto exit;
                    }
                );
                is_result_generated = true;
            }

            GW_IF_FAILED(
                subs_ctx->callback(
                    /* subscribe_cxt */ subs_ctx,
                    /* new_value */ result.serialize(),
                    /* old_value */ ""
                ),
                retval,
                {
                    GW_WARN_C("failed to execute callback, set failed: table_name(%s)", table_name.c_str());
                    goto exit;
                }
            );
        }
    } else {
        GW_WARN_C("failed to update row in SQL database: table_name(%s), sets(%s), where_clause(%s)",
                  table_name.c_str(), sets.c_str(), where_clause.c_str());
    }

exit:
    return retval;
}


gw_retval_t GWUtilSQLDatabase::update_rows(
    const std::string &table_name,
    const std::vector<std::pair<std::string, std::map<std::string, nlohmann::json>>> &rows
){
    gw_retval_t retval = GW_SUCCESS;
    std::string sql;
    GWUtilSqlQueryResult result;
    bool is_result_generated = false;
    std::vector<gw_db_subscribe_context_t*> list_subs_ctx = {};

    std::lock_guard<std::mutex> lock_callback(this->_callback_mutex);
    std::lock_guard<std::mutex> lock_db(this->_db_mutex);

    // helper: escape single quotes
    auto __escape_string = [](const std::string &input) {
        std::string output;
        output.reserve(input.size());
        for (char c : input) {
            if (c == '\'') output += "''";
            else output += c;
        }
        return output;
    };

    if (unlikely(rows.empty())) { goto exit; }

    // begin transaction
    retval = this->__execute("BEGIN;");
    if (unlikely(retval != GW_SUCCESS)) {
        GW_WARN_C("failed to begin transaction for update_rows: table_name(%s)", table_name.c_str());
        goto exit;
    }

    // execute each update
    for (const auto &row_pair : rows) {
        const std::string &where_clause = row_pair.first;
        const std::map<std::string, nlohmann::json> &data = row_pair.second;

        if (data.empty()) {
            // nothing to set for this row -> skip
            continue;
        }

        std::string sets;
        for (auto it = data.begin(); it != data.end(); ++it) {
            sets += it->first + "='" + __escape_string(it->second.dump()) + "',";
        }
        if (!sets.empty()) sets.pop_back();

        sql = "UPDATE " + table_name + " SET " + sets;
        if (!where_clause.empty()) {
            sql += " WHERE " + where_clause;
        }
        sql += ";";

        retval = this->__execute(sql);
        if (unlikely(retval != GW_SUCCESS)) {
            GW_WARN_C("failed to execute update_rows single update: table_name(%s), sql(%s)",
                      table_name.c_str(), sql.c_str());
            // rollback transaction
            this->__execute("ROLLBACK;");
            goto exit;
        }
    }

    // commit transaction
    retval = this->__execute("COMMIT;");
    if (unlikely(retval != GW_SUCCESS)) {
        GW_WARN_C("failed to commit transaction for update_rows: table_name(%s)", table_name.c_str());
        // attempt rollback
        this->__execute("ROLLBACK;");
        goto exit;
    }

    // success -> debug and trigger callbacks similar to update_row
    GW_DEBUG_C("update rows in SQL database: table_name(%s), num_rows(%zu)",
               table_name.c_str(), rows.size());

    // collect subscribers and run callbacks, providing whole-table result once
    this->__get_all_subscribers(nullptr, "", table_name, /* uri_precise_match */ true, list_subs_ctx);
    for (auto &subs_ctx : list_subs_ctx) {
        if (unlikely(!is_result_generated)) {
            sql = "SELECT * FROM " + table_name + ";";
            result.clear();
            GW_IF_FAILED(
                this->query_lockless(sql, result),
                retval,
                {
                    GW_WARN_C(
                        "failed to query all result from database before executing callback: table_name(%s), sql(%s)",
                        table_name.c_str(), sql.c_str()
                    );
                    // note: we already committed; just return error
                    goto exit;
                }
            );
            is_result_generated = true;
        }

        GW_IF_FAILED(
            subs_ctx->callback(
                /* subscribe_cxt */ subs_ctx,
                /* new_value */ result.serialize(),
                /* old_value */ ""
            ),
            retval,
            {
                GW_WARN_C("failed to execute callback in update_rows, table_name(%s)", table_name.c_str());
                goto exit;
            }
        );
    }

exit:
    return retval;
}


gw_retval_t GWUtilSQLDatabase::query(const std::string &sql, GWUtilSqlQueryResult& result){
    gw_retval_t retval = GW_SUCCESS;
    std::lock_guard<std::mutex> lock_callback(this->_callback_mutex);
    std::lock_guard<std::mutex> lock_db(this->_db_mutex);

    return this->query_lockless(sql, result);
}


gw_retval_t GWUtilSQLDatabase::query_lockless(const std::string &sql, GWUtilSqlQueryResult& result){
    gw_retval_t retval = GW_SUCCESS;
    char *errmsg = nullptr;
    int sdk_retval;

    auto callback = [](void *ctx, int argc, char **argv, char **azColName) -> int {
        GWUtilSqlQueryResult *res = static_cast<GWUtilSqlQueryResult *>(ctx);
        std::vector<std::string> row;
        int i;

        GW_CHECK_POINTER(res);
        if (res->columns.empty()) {
            for (int i = 0; i < argc; i++) {
                res->columns.emplace_back(azColName[i]);
            }
        }
        
        for (i = 0; i < argc; i++) {
            row.emplace_back(argv[i] ? argv[i] : "");
        }
        res->rows.emplace_back(std::move(row));

        return 0;
    };

    GW_IF_SQLITE_FAILED(
        sqlite3_exec(this->_db, sql.c_str(), callback, &result, &errmsg),
        sdk_retval,
        {
            GW_WARN_C(
                "failed to execute sqlite query: sql(%s), error(%s)",
                sql.c_str(), std::string(errmsg).c_str()
            );
            retval = GW_FAILED_SDK;
        }
    );

    if(unlikely(retval != GW_SUCCESS)){
        GW_WARN_C("failed to execute sqlite query: sql(%s)", sql.c_str());
    }

exit:
    return retval;
}


gw_retval_t GWUtilSQLDatabase::__execute(const std::string &sql){
    gw_retval_t retval = GW_SUCCESS;
    char *errmsg = nullptr;
    int sdk_retval;

    GW_IF_SQLITE_FAILED(
        sqlite3_exec(this->_db, sql.c_str(), nullptr, nullptr, &errmsg),
        sdk_retval,
        {
            retval = GW_FAILED_SDK;
            GW_WARN_C(
                "failed to execute sqlite query: sql(%s), error(%s)",
                sql.c_str(), std::string(errmsg).c_str()
            );
        }
    );

exit:
    if(errmsg != nullptr){
        sqlite3_free(errmsg);
    }
    return retval;
}
