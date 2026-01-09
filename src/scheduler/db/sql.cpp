#include <iostream>
#include <string>

#include "common/common.hpp"
#include "common/log.hpp"
#include "scheduler/scheduler.hpp"


gw_retval_t GWScheduler::__init_sql(){
    gw_retval_t retval = GW_SUCCESS;

    /* ==================== Capsule Management ==================== */
    // table for store capsule information
    GW_IF_FAILED(
        this->_db_sql.create_table(
            /* table_name */    "mgnt_capsule",
            /* schema */        "("
                                    "global_id          TEXT UNIQUE PRIMARY KEY, "
                                    "cpu_global_id      TEXT NOT NULL, "
                                    "start_time         TEXT NOT NULL, "
                                    "start_tsc          DOUBLE NOT NULL, "
                                    "end_time           TEXT, "
                                    "state              TEXT NOT NULL, "
                                    "kernel_version     TEXT NOT NULL, "
                                    "os_distribution    TEXT NOT NULL, "
                                    "pid                INTEGER NOT NULL, "
                                    "ip_addr            TEXT NOT NULL, "
                                    #ifdef GW_BACKEND_CUDA
                                    "cuda_rt_version    INTEGER NOT NULL, "
                                    "cuda_dv_version    INTEGER NOT NULL"
                                    #endif
                                ")"
        ),
        retval,
        {
            GW_WARN_C("failed to create table: table_name(mgnt_capsule), error(%s)", gw_retval_str(retval));
            goto exit;
        }
    );

    // table for store CPU information
    GW_IF_FAILED(
        this->_db_sql.create_table(
            /* table_name */    "mgnt_cpu",
            /* schema */        "("
                                    "global_id          TEXT UNIQUEPRIMARY KEY, "
                                    "cpu_name           TEXT NOT NULL, "
                                    "ip_addr            TEXT NOT NULL, "
                                    "tsc_freq           DOUBLE NOT NULL, "
                                    "num_cpu_cores      INTEGER NOT NULL, "
                                    "num_numa_nodes     INTEGER NOT NULL, "
                                    "dram_size          INTEGER NOT NULL"
                                ")"
        ),
        retval,
        {
            GW_WARN_C("failed to create table: table_name(mgnt_cpu), error(%s)", gw_retval_str(retval));
            goto exit;
        }
    );

    // table for store GPU information
    GW_IF_FAILED(
        this->_db_sql.create_table(
            /* table_name */    "mgnt_gpu",
            /* schema */        "("
                                    "global_id          TEXT UNIQUE PRIMARY KEY, "
                                    "cpu_global_id      TEXT NOT NULL, "
                                    "pcie_bus_id        TEXT NOT NULL, "
                                    "chip_name          TEXT NOT NULL, "
                                    "macro_arch         INTEGER NOT NULL, "
                                    "micro_arch         INTEGER NOT NULL, "
                                    "num_SMs            INTEGER NOT NULL, "
                                    "hbm_size           INTEGER NOT NULL"
                                ")"
        ),
        retval,
        {
            GW_WARN_C("failed to create table: table_name(mgnt_gpu), error(%s)", gw_retval_str(retval));
            goto exit;
        }
    );

    // table for store mappings of capsule-gpu
    GW_IF_FAILED(
        this->_db_sql.create_table(
            /* table_name */    "mgnt_capsule_gpu",
            /* schema */        "("
                                    // metadata
                                    "capsule_global_id  TEXT NOT NULL, "
                                    "gpu_global_id      TEXT NOT NULL, "
                                    "gpu_local_id       INTEGER NOT NULL, "
                                    "UNIQUE(capsule_global_id, gpu_global_id, gpu_local_id) ON CONFLICT IGNORE"
                                ")"
        ),
        retval,
        {
            GW_WARN_C("failed to create table: table_name(mgnt_capsule_gpu), error(%s)", gw_retval_str(retval));
            goto exit;
        }
    );

    // table for store mappings of cpu-gpu
    GW_IF_FAILED(
        this->_db_sql.create_table(
            /* table_name */    "mgnt_cpu_gpu",
            /* schema */        "("
                                    // metadata
                                    "cpu_global_id      TEXT NOT NULL, "
                                    "gpu_global_id      TEXT NOT NULL, "
                                    "UNIQUE(cpu_global_id, gpu_global_id) ON CONFLICT IGNORE"
                                ")"
        ),
        retval,
        {
            GW_WARN_C("failed to create table: table_name(mgnt_cpu_gpu), error(%s)", gw_retval_str(retval));
            goto exit;
        }
    );
    /* ==================== Capsule Management ==================== */

    /* ==================== Trace Management ==================== */
    // table for store trace result
    GW_IF_FAILED(
        this->_db_sql.create_table(
            /* table_name */    "mgnt_trace",
            /* schema */        "("
                                    "global_id          TEXT UNIQUE PRIMARY KEY, "
                                    "target             TEXT NOT NULL, "
                                    "type               TEXT NOT NULL"
                                ")"
        ),
        retval,
        {
            GW_WARN_C("failed to create table: table_name(mgnt_trace), error(%s)", gw_retval_str(retval));
            goto exit;
        }
    );

    // table for store relationships between trace results
    GW_IF_FAILED(
        this->_db_sql.create_table(
            /* table_name */    "mgnt_trace_childtrace",
            /* schema */        "("
                                    "global_id          TEXT NOT NULL, "
                                    "child_global_id    TEXT NOT NULL, "
                                    "UNIQUE(global_id, child_global_id) ON CONFLICT IGNORE"
                                ")"
        ),
        retval,
        {
            GW_WARN_C("failed to create table: table_name(mgnt_trace_childtrace), error(%s)", gw_retval_str(retval));
            goto exit;
        }
    );
    /* ==================== Trace Management ==================== */

    /* ==================== Agent Management ==================== */
    // table for store agent context
    GW_IF_FAILED(
        this->_db_sql.create_table(
            /* table_name */    "mgnt_agent_context",
            /* schema */        "("
                                    "global_id TEXT NOT NULL UNIQUE PRIMARY KEY"
                                ")"
        ),
        retval,
        {
            GW_WARN_C("failed to create table: table_name(mgnt_agent_context), error(%s)", gw_retval_str(retval));
            goto exit;
        }
    );

    // table for store agent context node
    GW_IF_FAILED(
        this->_db_sql.create_table(
            /* table_name */    "mgnt_agent_task",
            /* schema */        "("
                                    "global_id          TEXT NOT NULL UNIQUE PRIMARY KEY, "
                                    "context_global_id  TEXT NOT NULL,"
                                    "status             TEXT NOT NULL"
                                ")"
        ),
        retval,
        {
            GW_WARN_C("failed to create table: table_name(mgnt_agent_task), error(%s)", gw_retval_str(retval));
            goto exit;
        }
    );

    // table for store agent context node topology
    GW_IF_FAILED(
        this->_db_sql.create_table(
            /* table_name */    "mgnt_agent_task_topo",
            /* schema */        "("
                                    "parent_global_id  TEXT NOT NULL,"
                                    "child_global_id   TEXT NOT NULL,"
                                    "UNIQUE(parent_global_id, child_global_id) ON CONFLICT IGNORE"
                                ")"
        ),
        retval,
        {
            GW_WARN_C("failed to create table: table_name(mgnt_agent_task_topo), error(%s)", gw_retval_str(retval));
            goto exit;
        }
    );

    // table for store agent context node topology
    GW_IF_FAILED(
        this->_db_sql.create_table(
            /* table_name */    "mgnt_agent_content",
            /* schema */        "("
                                    "global_id          TEXT NOT NULL UNIQUE PRIMARY KEY, "
                                    "context_global_id  TEXT NOT NULL,"
                                    "task_global_id     TEXT NOT NULL,"
                                    "type               INTEGER NOT NULL,"
                                    "metadata           TEXT"
                                ")"
        ),
        retval,
        {
            GW_WARN_C("failed to create table: table_name(mgnt_agent_content), error(%s)", gw_retval_str(retval));
            goto exit;
        }
    );
    /* ==================== Agent Management ==================== */


    /* ==================== Kernel Management ==================== */
    #if GW_BACKEND_CUDA
        // table for store kernel overview
        GW_IF_FAILED(
            this->_db_sql.create_table(
                /* table_name */    "mgnt_cuda_kernel_overview",
                /* schema */        "("
                                        "mangled_name                   TEXT NOT NULL UNIQUE PRIMARY KEY, "
                                        "cu_function                    TEXT NOT NULL, "
                                        "arch                           TEXT NOT NULL"
                                    ")"
            ),
            retval,
            {
                GW_WARN_C("failed to create table: table_name(mgnt_cuda_kernel_overview), error(%s)", gw_retval_str(retval));
                goto exit;
            }
        );

        // table for store kernel
        GW_IF_FAILED(
            this->_db_sql.create_table(
                /* table_name */    "mgnt_cuda_kernel",
                /* schema */        "("
                                        "mangled_name                       TEXT NOT NULL UNIQUE PRIMARY KEY, "
                                        "cu_function                        TEXT NOT NULL, "
                                        "arch                               TEXT NOT NULL, "
                                        "list_instructions                  TEXT NOT NULL, "
                                        "list_basic_blocks                  TEXT NOT NULL, "
                                        "list_reg_trace                     TEXT NOT NULL, "
                                        "list_uniform_reg_trace             TEXT NOT NULL, "
                                        "list_predicate_reg_trace           TEXT NOT NULL, "
                                        "list_uniform_predicate_reg_trace   TEXT NOT NULL"
                                    ")"
            ),
            retval,
            {
                GW_WARN_C("failed to create table: table_name(mgnt_cuda_kernel), error(%s)", gw_retval_str(retval));
                goto exit;
            }
        );
    #endif

 exit:
    return retval;
}
