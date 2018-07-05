#include "MySQLInterface.hh"

#include <mysql++.h>

#include <sstream>

static VDatabaseInterface::Factory<MySQLInterface> __mysqlfactory("MySQL");

MySQLInterface::MySQLInterface(const std::string& db,
                               const std::string& tbl,
                               const std::string& hostname,
                               int hostport,
                               const std::string& username,
                               const std::string& pswd) :
                               VDatabaseInterface("MySQL")
{
    Configure(db, tbl, hostname, hostport, username, pswd);

    RegisterParameter("host", host, "Host where MySQL server is running");
    RegisterParameter("port", port, "Port where MySQL server is running on host");
    RegisterParameter("database", database, "Name of database where runinfo is stored");
    RegisterParameter("table", table, "Name of table where runinfo is stored");
    RegisterParameter("user", user, "Username for authentication");
    RegisterParameter("password", password, "Password for authentication");
}

MySQLInterface::~MySQLInterface() { }

void MySQLInterface::Configure(const std::string& db,
                               const std::string& tbl,
                               const std::string& hostname,
                               int hostport,
                               const std::string& username,
                               const std::string& pswd)
{
    host = hostname;
    port = hostport;
    database = db;
    table = tbl;
    user = username;
    password = pswd;
}

int MySQLInterface::Connect()
{
    if(_connected) { return 0; }
    std::string error = "";
    try{
        mysqlconn.connect(database.c_str(), host.c_str(), user.c_str(), password.c_str(), port);
    }
    catch(const std::exception& e){
        Message(ERROR) << "MySQLInterface::Connect caught exception "
            << e.what() << " while trying to connect.\n";
        return 1;
    }

    Message(DEBUG) << "Successfully connected to MySQL database " << database
        << " at " << host << ":" << port << "\n";
    _connected = true;
    return 0;
}

int MySQLInterface::Disconnect()
{
    return 0;
}

runinfo MySQLInterface::LoadRuninfo(long runid)
{
    std::stringstream ss;
    ss << "{ runid: " << runid << " }";
    return LoadRuninfo(ss.str());
}

runinfo MySQLInterface::LoadRuninfo(const std::string& query)
{
    std::vector<runinfo> v;
    if(LoadRuninfo(v, query) >= 0 && v.size() > 0) { return v[0]; }
    return runinfo(-1);
}

int MySQLInterface::LoadRuninfo(std::vector<runinfo>& vec, const std::string& query)
{
    mysqlpp::Query q = mysqlconn.query();
    q << query;
    return LoadRuninfo(vec, mysqlpp::Query(q));
}

runinfo MySQLInterface::LoadRuninfo(const mysqlpp::Query& query)
{
    std::vector<runinfo> v;
    if(LoadRuninfo(v, query) <= 0 && v.size() > 0) { return v[0]; }
    return runinfo(-1);
}

int MySQLInterface::LoadRuninfo(std::vector<runinfo>& vec, const mysqlpp::Query& query)
{
    if(!_connected && Connect() != 0) { return -1; }

    vec.clear();

    return vec.size();
}

  /*Cursor cursor = _dbconnection.query(GetNS(),query);
  while(cursor->more()){
    BSONObj obj = cursor->next();
    runinfo info = RuninfoFromBSONObj(obj);
    vec.push_back(info );
  }*/

int MySQLInterface::StoreRuninfo(runinfo* info, STOREMODE mode)
{
    if(!_connected && Connect() != 0) { return -1; }

    mysqlpp::Query q = mysqlconn.query();

    std::cout << "Storing run info..." << std::endl;

    try{
        switch(mode){
        case INSERT:
            Message(INFO)<<"DB Insert mode not functional.\n";
            q << "INSERT INTO " << table << " (runid, starttime, endtime, events) VALUES (" 
              << info->runid << ", FROM_UNIXTIME("
              << info->starttime << "), FROM_UNIXTIME("
              << info->endtime << "), "
              << info->events
              << ");" ;
           q.exec();
           break;
       case UPDATE:
            Message(INFO)<<"DB Update mode not functional.\n";
            q << "UPDATE" << table << "SET"
              << " runid = " << info->runid
              << " starttime = FROM_UNIXTIME(" << info->starttime << ")"
              << " endtime = FROM_UNIXTIME(" << info->endtime << ")"
              << " events = " << info->events
              << " WHERE runid = " << info->runid << ";";
              q.exec();
            break;
        case REPLACE:
            Message(INFO)<<"DB Replace mode not functional.\n";
            break;
        case UPSERT:
            std::cout << "Comment: " << info->metadata["comment"] << std::endl;
            q << "INSERT INTO " << table << " (runid, starttime, endtime, events, comment, n_channels) VALUES (" 
              << info->runid << ", FROM_UNIXTIME("
              << info->starttime << "), FROM_UNIXTIME("
              << info->endtime << "), "
              << info->events << ", "
              // << info->type << ", "
              << info->metadata["comment"] << ", "
              << info->metadata["nchans"]
              << ") ON DUPLICATE KEY UPDATE "
              << "runid = " << info->runid
              << ", starttime = FROM_UNIXTIME(" << info->starttime
              << "), endtime = FROM_UNIXTIME(" << info->endtime
              << "), events = " << info->events << ";";
            std::cout << q << std::endl;
            q.exec();
            break;
        default:
            Message(INFO)<<"Unknown db update mode supplied! Doing nothing.\n";
            break;
        }
    }
    catch(std::exception& e){
        Message(ERROR)<<"MySQLInterface::StoreRuninfo caught exception "
                      <<e.what()<<std::endl;
        return -3;
    }
    return 0;
}
