#ifndef MYSQLINTERFACE_h
#define MYSQLINTERFACE_h

#include "VDatabaseInterface.hh"

#include <mysql++.h>

class MySQLInterface: public VDatabaseInterface{
public:
    MySQLInterface(const std::string& db = "daqman",
                   const std::string& hostname = "localhost",
		   int hostport = 22);

    ~MySQLInterface();

    void Configure(const std::string& db = "daqman",
                   const std::string& hostname = "localhost",
                   int hostport = 22);

    int Connect(std::string user, std::string password);
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

    std::string runtable;
    std::string caltable;

    std::string writeuser;
    std::string writepassword;
    std::string readuser;
    std::string readpassword;

private:
    #ifndef __CINT__
    mysqlpp::Connection mysqlconn;
    #endif

    MySQLInterface& operator=(const MySQLInterface& right) { return *this; }
    MySQLInterface(const MySQLInterface& right) {}
};

#endif
