#define DUCKDB_EXTENSION_MAIN

#include "delta_extension.hpp"

#include "delta_utils.hpp"
#include "delta_functions.hpp"
#include "duckdb.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/main/extension_util.hpp"
#include "duckdb/storage/storage_extension.hpp"
#include "storage/delta_catalog.hpp"
#include "storage/delta_transaction_manager.hpp"
#include "duckdb/main/config.hpp"

namespace duckdb {

static unique_ptr<Catalog> DeltaCatalogAttach(StorageExtensionInfo *storage_info, ClientContext &context,
                                              AttachedDatabase &db, const string &name, AttachInfo &info,
                                              AccessMode access_mode) {

	auto res = make_uniq<DeltaCatalog>(db, info.path, access_mode);

	for (const auto &option : info.options) {
		if (StringUtil::Lower(option.first) == "pin_snapshot") {
			res->use_cache = option.second.GetValue<bool>();
		}
	}

	res->SetDefaultTable(DEFAULT_SCHEMA, DEFAULT_DELTA_TABLE);

	return std::move(res);
}

static unique_ptr<TransactionManager> CreateTransactionManager(StorageExtensionInfo *storage_info, AttachedDatabase &db,
                                                               Catalog &catalog) {
	auto &delta_catalog = catalog.Cast<DeltaCatalog>();
	return make_uniq<DeltaTransactionManager>(db, delta_catalog);
}

class DeltaStorageExtension : public StorageExtension {
public:
	DeltaStorageExtension() {
		attach = DeltaCatalogAttach;
		create_transaction_manager = CreateTransactionManager;
	}
};

static void LoadInternal(DatabaseInstance &instance) {
	// Load Table functions
	for (const auto &function : DeltaFunctions::GetTableFunctions(instance)) {
		ExtensionUtil::RegisterFunction(instance, function);
	}

	// Load Scalar functions
	for (const auto &function : DeltaFunctions::GetScalarFunctions(instance)) {
		ExtensionUtil::RegisterFunction(instance, function);
	}

	// Register the "single table" delta catalog (to ATTACH a single delta table)
	auto &config = DBConfig::GetConfig(instance);
	config.storage_extensions["delta"] = make_uniq<DeltaStorageExtension>();

	config.AddExtensionOption("delta_scan_explain_files_filtered",
	                          "Adds the filtered files to the explain output. Warning: this may impact performance of "
	                          "delta scan during explain analyze queries.",
	                          LogicalType::BOOLEAN, Value(true));

	config.AddExtensionOption(
	    "delta_kernel_logging",
	    "Forwards the internal logging of the Delta Kernel to the duckdb logger. Warning: this may impact "
	    "performance even with DuckDB logging disabled.",
	    LogicalType::BOOLEAN, Value(false), LoggerCallback::DuckDBSettingCallBack);

	LoggerCallback::Initialize(instance);
}

void DeltaExtension::Load(DuckDB &db) {
	LoadInternal(*db.instance);
}

std::string DeltaExtension::Name() {
	return "delta";
}

} // namespace duckdb

extern "C" {

DUCKDB_EXTENSION_API void delta_init(duckdb::DatabaseInstance &db) {
	duckdb::DuckDB db_wrapper(db);
	db_wrapper.LoadExtension<duckdb::DeltaExtension>();
}

DUCKDB_EXTENSION_API const char *delta_version() {
	return duckdb::DuckDB::LibraryVersion();
}
}

#ifndef DUCKDB_EXTENSION_MAIN
#error DUCKDB_EXTENSION_MAIN not defined
#endif
