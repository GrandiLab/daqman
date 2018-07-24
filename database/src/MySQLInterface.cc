#include "MySQLInterface.hh"

#include <typeinfo>
#include <mysql++.h>

#include <sstream>

static VDatabaseInterface::Factory<MySQLInterface> __mysqlfactory("MySQL");

MySQLInterface::MySQLInterface(const std::string& db,
                               const std::string& hostname,
                               int hostport) :
                               VDatabaseInterface("MySQL")
{
    Configure(db, hostname, hostport);

    RegisterParameter("host", host, "Host where MySQL server is running");
    RegisterParameter("port", port, "Port where MySQL server is running on host");
    RegisterParameter("database", database, "Name of database where runinfo is stored");
    RegisterParameter("runtable", runtable = "", "Name of run table where runinfo is stored");
    RegisterParameter("caltable", caltable = "", "Name of calibration table where calibration data is stored");
    RegisterParameter("writeuser", writeuser = "", "Username for write authentication");
    RegisterParameter("writepassword", writepassword = "", "Password for write authentication");
    RegisterParameter("readuser", readuser = "", "Username for read authentication");
    RegisterParameter("readpassword", readpassword = "", "Password for read authentication");
}

MySQLInterface::~MySQLInterface() { }

void MySQLInterface::Configure(const std::string& db,
                               const std::string& hostname,
                               int hostport)
{
    host = hostname;
    port = hostport;
    database = db;
}

int MySQLInterface::Connect(std::string user, std::string password)
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
    if(_connected)
    {
        mysqlconn.disconnect();

        Message(DEBUG) << "Successfully disconnected from MySQL database " << database << " at " << host << ":" << port << "\n";
        _connected = false;
        return 0;
    }

    Message(DEBUG) << "Not connected to a MySQL database" << "\n";
    return 0;
}

runinfo MySQLInterface::LoadRuninfo(long runid)
{
    std::stringstream ss;
    ss << "SELECT runid, UNIX_TIMESTAMP(starttime), UNIX_TIMESTAMP(endtime), events, comment, n_channels FROM " << runtable << " WHERE runid = " << runid;
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
    return LoadRuninfo(vec, q);
}

runinfo MySQLInterface::LoadRuninfo(mysqlpp::Query& query)
{
    std::vector<runinfo> v;
    if(LoadRuninfo(v, query) <= 0 && v.size() > 0) { return v[0]; }
    return runinfo(-1);
}

int MySQLInterface::LoadRuninfo(std::vector<runinfo>& vec, mysqlpp::Query& query)
{
    if(Connect(readuser, readpassword) != 0) { return -1; }

    vec.clear();

    mysqlpp::StoreQueryResult result = query.store();

    for(unsigned int index = 0; index < result.num_rows(); ++index)
    {
        runinfo info = RowToRuninfo(result[index]);

	    mysqlpp::Query q1 = mysqlconn.query();
        q1 << "SELECT runid, ABS(UNIX_TIMESTAMP(starttime) - " << info.starttime << ") AS dt FROM " << runtable << " WHERE type = 'laser' ORDER BY dt ASC LIMIT 1;";
        mysqlpp::StoreQueryResult q1result = q1.store();

        mysqlpp::Query q2 = mysqlconn.query();
        q2 << "SELECT channel, spe_mean FROM " << caltable << " WHERE runid = " << q1result[0]["runid"] << " ORDER BY channel;";
        mysqlpp::StoreQueryResult q2result = q2.store();

        for(unsigned int i = 0; i < q2result.num_rows(); ++i)
        {
            std::map<std::string, std::string> chandata;
            chandata["spe_mean"] = std::string(q2result[i]["spe_mean"]);

            std::cout << "Channel ID: " << q2result[i]["channel"] << std::endl;
            std::cout << "SPE Mean: " << q2result[i]["spe_mean"] << std::endl;

            info.channel_metadata[q2result[i]["channel"]] = chandata;
        }

        vec.push_back(info);
    }

    Disconnect();

    return vec.size();
}

runinfo MySQLInterface::RowToRuninfo(mysqlpp::Row row)
{
    runinfo info;

    try{
        mysqlpp::Row::const_iterator it;
        info.runid = row["runid"];
        info.starttime = row["UNIX_TIMESTAMP(starttime)"];
        info.endtime = row["UNIX_TIMESTAMP(endtime)"];
        info.events = row["events"];
        info.metadata["comment"] = std::string(row["comment"].data(), row["comment"].length());
        info.metadata["nchans"] = std::string(row["n_channels"].data(), row["n_channels"].length());
    }

    catch(std::exception& e){
        Message(ERROR) << "Unable to load runinfo: " << e.what() << "\n";
    }

    return info;
}

int MySQLInterface::StoreRuninfo(runinfo* info, STOREMODE mode)
{
    if(Connect(writeuser, writepassword) != 0) { return -1; }

    mysqlpp::Query q = mysqlconn.query();

    try{
        switch(mode){
        case INSERT:
            Message(INFO)<<"DB Insert mode not functional.\n";
            q << "INSERT INTO " << runtable << " (runid, starttime, endtime, events) VALUES (" 
              << info->runid << ", FROM_UNIXTIME("
              << info->starttime << "), FROM_UNIXTIME("
              << info->endtime << "), "
              << info->events
              << ");" ;
           q.exec();
           break;
       case UPDATE:
            Message(INFO)<<"DB Update mode not functional.\n";
            q << "UPDATE" << runtable << "SET"
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
            q << "INSERT INTO " << runtable << " (runid, type, starttime, endtime, events, comment, n_channels) VALUES (" 
              << info->runid << ", '"
              << info->type << "', FROM_UNIXTIME("
              << info->starttime << "), FROM_UNIXTIME("
              << info->endtime << "), "
              << info->events << ", "
              // << info->type << ", "
              << info->metadata["comment"] << ", "
              << info->metadata["nchans"]
              << ") ON DUPLICATE KEY UPDATE "
              << "runid = " << info->runid
              << ", type = '" << info->type
              << "', starttime = FROM_UNIXTIME(" << info->starttime
              << "), endtime = FROM_UNIXTIME(" << info->endtime
              << "), events = " << info->events
              << ", comment = " << info->metadata["comment"]
              << ", n_channels = " << info->metadata["nchans"] << ";";
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
        Disconnect();
        return -3;
    }

    Disconnect();

    return 0;
}

int MySQLInterface::StoreChannelinfo(unsigned int runid, unsigned int channelid, double spemean, double occupancy, STOREMODE mode)
{
    if(Connect(writeuser, writepassword) != 0) { return -1; }

    mysqlpp::Query q = mysqlconn.query();

    try{
        switch(mode){
        case UPSERT:
            q << "INSERT INTO " << caltable << " (runid, channel, spe_mean, lambda) VALUE ("
              << runid << ", "
              << channelid << ", "
              << spemean << ", "
              << occupancy << ") ON DUPLICATE KEY UPDATE "
              << "runid = " << runid << ", "
              << "channel = " << channelid << ", "
              << "spe_mean = " << spemean << ","
              << "lambda = " << occupancy << ";";
            q.exec();
            break;
        default:
            Message(INFO)<<"Unknown db update mode supplied! Doing nothing.\n";
            break;
        }
    }
    catch(std::exception& e){
        Message(ERROR)<<"MySQLInterface::StoreChannelinfo caught exception "
                      <<e.what()<<std::endl;
        Disconnect();
        return -3;
    }

    Disconnect();

    return 0;
}
