#pragma once

#include <iostream>
#include <map>
#include <vector>
#include <string>
#include <mutex>

#include "sqlite3.h"

#include "common/common.hpp"
#include "common/log.hpp"
#include "common/utils/database.hpp"

#include <nlohmann/json.hpp>


class GWUtilSqlQueryResult {
 public:
    GWUtilSqlQueryResult(){};
    ~GWUtilSqlQueryResult() = default;

    // column names
    std::vector<std::string> columns;

    // row values
    std::vector<std::vector<std::string>> rows;


    /*!
     *  \brief  serialize the query result to json
     *  \return json object
     */
    nlohmann::json serialize() {
        nlohmann::json object;
        object["columns"] = this->columns;
        object["rows"] = this->rows;
        return object;
    }


    /*!
     *  \brief  deserialize the query result from json object
     *  \param  json object
     *  \return GW_SUCCESS for successfully deserialized
     */
    void deserialize(const nlohmann::json& json) {
        if(json.contains("columns"))
            this->columns = json["columns"];
        if(json.contains("rows"))
            this->rows = json["rows"];
    }


    /*!
     *  \brief clear the query result
     */
    inline void clear() {
        this->columns.clear();
        this->rows.clear();
    }
};


#define GW_IF_SQLITE_FAILED(api_call, retval, callback) \
    __GW_IF_FAILED(api_call, retval, callback, SQLITE_OK)


class GWUtilSQLDatabase final : public GWUtilDatabase {
 public:
    /*!
     *  \brief  constructor
     *  \param  directory  directory to store the sqlite database file
     */
    GWUtilSQLDatabase(const std::string directory="/tmp");
    

    /*!
     *  \brief  destructor
     */
    ~GWUtilSQLDatabase();

    
    /*!
     *  \brief  create new table in the database
     *  \param  table_name  name of the table
     *  \param  schema     schema of the table, e.g., (id INTEGER PRIMARY KEY, name TEXT)
     *  \return GW_SUCCESS for successfully created
     */
    gw_retval_t create_table(const std::string &table_name, const std::string &schema);


    /*!
     *  \brief  drop table in the database
     *  \param  table_name  name of the table
     *  \return GW_SUCCESS for successfully dropped
     */
    gw_retval_t drop_table(const std::string &table_name);


    /*!
     *  \brief  insert row into the table
     *  \param  table_name  name of the table
     *  \param  data        data to be inserted
     *  \return GW_SUCCESS for successfully inserted
     */
    gw_retval_t insert_row(const std::string &table_name, const std::map<std::string, nlohmann::json> &data);


    /*!
     *  \brief  delete row from the table
     *  \param  table_name  name of the table
     *  \param  where_clause  where clause to filter the rows
     *  \return GW_SUCCESS for successfully deleted
     */
    gw_retval_t delete_row(const std::string &table_name, const std::string &where_clause);


    /*!
     *  \brief  update row in the table
     *  \param  table_name      name of the table
     *  \param  data            data to be updated
     *  \param  where_clause    where clause to filter the rows
     *  \return GW_SUCCESS for successfully updated
     */
    gw_retval_t update_row(
        const std::string &table_name, const std::map<std::string, nlohmann::json> &data, const std::string &where_clause
    );


    /*!
     *  \brief  update multiple rows in the table
     *  \param  table_name      name of the table
     *  \param  rows            rows to be updated, each row is a pair of where clause and data
     *  \return GW_SUCCESS for successfully updated
     */
    gw_retval_t update_rows(
        const std::string &table_name,
        const std::vector<std::pair<std::string, std::map<std::string, nlohmann::json>>> &rows
    );


    /*!
     *  \brief  query data from the table
     *  \param  sql        sql query statement
     *  \param  result     result of the query
     *  \return GW_SUCCESS for successfully queried
     */
    gw_retval_t query(const std::string &sql, GWUtilSqlQueryResult& result);


    /*!
     *  \brief  query data from the table (lockless)
     *  \note   this function is provided for subscribe initializing function 
     *  \param  sql        sql query statement
     *  \param  result     result of the query
     *  \return GW_SUCCESS for successfully queried
     */
    gw_retval_t query_lockless(const std::string &sql, GWUtilSqlQueryResult& result);


 private:
    // sqlite database handle
    sqlite3 *_db = nullptr;

    // filename of the sqlite database
    std::string _filename = "";


    /*!
     *  \brief  execute sql statement
     *  \param  sql        sql statement
     *  \return GW_SUCCESS for successfully executed
     */
    gw_retval_t __execute(const std::string &sql);
};
