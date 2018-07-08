#ifndef MYSQLINTERFACE_h
#define MYSQLINTERFACE_h

#include "VDatabaseInterface.hh"

#include <mysql++.h>

class MySQLInterface: public VDatabaseInterface{
public:
    MySQLInterface(const std::string& db = "daqman",
                   const std::string& tbl = "runinfo",
                   const std::string& hostname = "localhost",
                   int hostport = 22,
                   const std::string& username = "",
                   const std::string& pswd = "");

    ~MySQLInterface();

    void Configure(const std::string& db = "daqman",
                   const std::string& tbl = "runinfo",
                   const std::string& hostname = "localhost",
                   int hostport = 22,
                   const std::string& username = "",
                   const std::string& pswd = "");

    int Connect();
    int Disconnect();

    runinfo LoadRuninfo(long runid);
    runinfo LoadRuninfo(const std::string& query);
    int LoadRuninfo(std::vector<runinfo>& vec, const std::string& query);

    int StoreRuninfo(runinfo* runinfo, STOREMODE mode = UPSERT);

    #ifndef __CINT__
    runinfo LoadRuninfo(mysqlpp::Query& query);
    int LoadRuninfo(std::vector<runinfo>& vec, mysqlpp::Query& query);
    runinfo RowToRuninfo(mysqlpp::Row row);
    #endif

    std::string host;
    int port;
    std::string database;
    std::string table;
    std::string user;
    std::string password;

private:
    #ifndef __CINT__
    mysqlpp::Connection mysqlconn;
    #endif

    MySQLInterface& operator=(const MySQLInterface& right) { return *this; }
    MySQLInterface(const MySQLInterface& right) {}
};

#endif
