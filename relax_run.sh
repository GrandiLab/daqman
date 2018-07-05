#!/bin/bash
#set -e

#changes to work without RAID array:
# 1) make rawdata, processed_data, test_processing directories in /incoming
# 2) make symlinks from /data/ to these locations
# 3) change temp_dir from /incoming to /incoming/rawdata
# 4) remove all lines where sync_script is run
# 5) remove the line that automatically cleans up runs in /incoming directory
# 6) disable mysql, rsnapshot backups to /data directory

#---- declare useful variables here --------
directory=/data/relax/rawdata #RELAX: change back to /data/rawdata
temp_dir=/data/relax/incoming/rawdata #RELAX: change back to /incoming/rawdata

#Switched for RELAX to local check (LG - Feb 2018)
#data_server=daqman@192.168.1.100
#db_readhost should be set to localhost if mysqlDB runs on same DAQ machine
db_readhost=localhost
db=relaxdb
#ensure password is stored to access the db without declaring it explicity (LG - Feb 2018)
db_readuser=analysis

program="./bin/daqman -v"
info_program="echo 'not running run_info'" #./bin/run_info --db-only"

laser_cfgfile=cfg/laserrun.cfg
darkbox_cfgfile=cfg/darkboxrun.cfg
regular_cfgfile=cfg/regularrun.cfg

flag_exit=0;
#--------------------------------------------

print_usage(){
    cat<<EOF
  Usage: $0 [-m "comment"] [--pretend] <runtype> [<type-specific-options>]
  Allowed runtypes are:
    laser
    darkbox
    regular
    custom
EOF
exit 0
}

#make the filename for the next run number, or exit if database out of sync
make_filename(){
  #number=$(ssh $data_server find $directory/ -name 'Run[0-9][0-9][0-9][0-9][0-9][0-9]*' | sort | egrep -o '[[:digit:]]{6}' | tail -n1 | sed -e 's/^0*//')
  number=$(find $temp_dir/ -name 'Run[0-9][0-9][0-9][0-9][0-9][0-9]' 2>/dev/null | sort | egrep -o '[[:digit:]]{6}' | tail -n1 | sed -e 's/^0*//')

  #find the number of the next run from the database
  dbnumber=$(echo "select runid from daqruns order by runid desc limit 1;" | mysql -u $db_readuser -h $db_readhost -p"$MYSQL_analysis_PASSWORD" $db | grep -v runid)
  if [ $number -eq $dbnumber ] ; then
  outfile="Run"`printf "%06d" $(( number + 1 ))`
  echo $outfile
else
  echo "Error"
  fi
}

#-----------------------
#---- start of main ----
echo "===============================================" >&2
echo "==== Welcome to Relax Detector DAQ ====" >&2
echo "====    `date`       ====" >&2
echo "===============================================" >&2
pretend=""
comment=""
while [ $# -gt 0 ]; do
    case "$1" in
	-m) comment="$2"; shift 2;;
	--pretend) pretend="--disable RawWriter"; shift;;
	-*) print_usage;;
	*) break;;
    esac
done

if [ $# -eq 0 ] ; then
    print_usage
fi

#find the allowed run types
allowed_types=$(echo "DESCRIBE daqruns;" | mysql -u $db_readuser -h $db_readhost -p"$MYSQL_analysis_PASSWORD" $db | grep 'type' | awk '{print $2}' | sed -e 's/^enum//')

#determine the main program arguments
program_type=$1
program_args=""
info_string=""
#RELAX: might be of interest later on
#./retrieve-fields.sh
#echo "Have you entered the fields above in the cfg file?"
#read -p "Press [Enter] to continue or [control]-c to exit..."

case "$program_type" in
    'laser')
	#laser runs take exactly 1 parameter: # of events
	if [ $# -eq 2 ] && [ $2 -eq $2 ] ; then
	    #$2 is a number, we're ok
	    program_args="--cfg $laser_cfgfile -e $2"
        info_string="type $1"
	else
	    echo "Incorrect number of arguments:" >&2
	    echo "Usage: $0 laser <# of triggers>" >&2
	    exit 1
	fi
	;;
    'darkbox')
	#darkbox runs take 3 parameters: # of events, PMT voltage, amplification
	if [ $# -eq 4 ] && [ $2 -eq $2 ] && \
	    [ $3 -eq $3 ] ; then
	    program_args="--cfg $darkbox_cfgfile -e $2"
	    info_string="insert_channel ( channel 0 voltage $3 amplification $4 )"
	else
	    echo "Incorrect number of arguments: " >&2
	    echo "Usage: $0 darkbox <# of triggers> <PMT Voltage> <PMT Amplification>" >&2
	    exit 2
	fi
#	;;
#	#autotrigger takes exactly 1 parameter: # of events
#	if [ $# -eq 2 ] && [ $2 -eq $2 ] ; then
#		#$2 is a number, we're ok
#	    program_args="--cfg $autotrigger_cfgfile -e $2"
#	else
#	    echo "Incorrect number of arguments:" >&2
#	    echo "Usage: $0 autotrigger <# of triggers>" >&2
#	    exit 3
#	fi
	;;
    'regular')
	#regular runs require runtype, time in seconds
	#if [ $# -eq 3 ] && [[ $allowed_types = *$2* ]] && [ $3 -eq $3 ] ; then
	if [ $# -eq 4 ] && [ $4 -eq $4 ] ; then
	    program_args="--cfg $regular_cfgfile $3 $4 "
	    info_string="type $2"
	else
	    echo "Incorrect arguments:" >&2
	    echo "Usage: $0 regular <runtype> -t <seconds to run>" >&2
	    echo "Usage: $0 regular <runtype> -e <# of triggers>" >&2
	    echo "Valid values for runtype are $allowed_types" >&2
	    exit 4
	fi
	;;
    'custom')
	#custom runs require a runtype
	#if [ $# -ge 2 ] && [[ $allowed_types = *$2* ]] ; then
	if [ $# -ge 2 ] ; then
	    info_string="type $2"
	    shift 2
	    program_args="$@"
	else
	    echo "Incorrect arguments:" >&2
	    echo "Usage: $0 custom <runtype> [<daqman options>]" >&2
	    echo "Valid values for runtype are $allowed_types" >&2
	    exit 5
	fi
	;;
    *)
	print_usage
	;;
esac

#Determine if run number is compatible between DB and last rawdata filename in incoming folder
number=$(find $temp_dir/ -name 'Run[0-9][0-9][0-9][0-9][0-9][0-9]' 2>/dev/null | sort | egrep -o '[[:digit:]]{6}' | tail -n1 | sed -e 's/^0*//')
dbnumber=$(echo "select runid from daqruns order by runid desc limit 1;" | mysql -u $db_readuser -h $db_readhost -p"$MYSQL_analysis_PASSWORD" $db | grep -v runid)
if [ $number -ne $dbnumber ] ; then
 echo -e "\e[1;31m!!!!!!!!! ERROR:\e[0m"
 echo -e "\e[1;31mThe latest run in $directory does't match the database!\e[0m"
 echo -e "\e[1;31mThe latest run number in $directory is $number \e[0m"
 echo -e "\e[1;31mThe latest run inserted into the database is $dbnumber \e[0m"
 echo -e "\e[1;31mYou must update the database or copy runs to $directory before proceeding! \e[0m"
exit 0
fi

 #determine the file names
 filenamebase=$(make_filename)
 if [ $? -ne 0 ] ; then exit ; fi

 echo `date "+%T"` "$filenamebase starting ..." >&2

 data_dir=${temp_dir}/${filenamebase}
 datafile=${temp_dir}/${filenamebase}
 cfgfile=${data_dir}/${filenamebase}.cfg

 #now that we've ok'd the arguments, ask for a comment if there isn't one
 if [ -z "$comment" ] ; then
     echo "Please enter a comment for this run:"
     read comment
 fi
#
#invoke the daq program
if [ -z "$pretend" ] ; then
    echo `date "+%T"` "Data files will be saved to $data_dir/" >&2
    echo `date "+%T"` "Cfg file will be saved to $cfgfile" >&2
    #echo "Saving output to $datafile"
    if [ -d $data_dir ] ; then
	echo "Run already exists!"
	exit 10
    fi
fi
info_string="comment \"$comment\" $info_string"
echo $program_args
echo $info_string
echo "$program $program_args --info "$info_string" -f $datafile $pretend"
$program $program_args --info "$info_string" -f $datafile $pretend

#$program $program_args -f $datafile $pretend
if [ -n "$pretend" ] ; then
    echo `date "+%T"` "Running in pretend mode, so exiting now..."
    exit 0
fi

#if the file was written, copy to data server. Otherwise clear the cfg file
if [ -d $data_dir ] ; then
    chmod -R a-w $data_dir/ 2>/dev/null
    echo `date "+%T"` "Data saved to $data_dir/." >&2
else
    if [ -f $cfgfile ] ; then
	echo `date "+%T"` "!!!!! Output file $datafile was not created!"
	echo "Do you want to remove the config file? [y/N]"
	read response
	if [ "$response" = "y" ] || [ "$response" = "Y" ] ; then
	    #rm -f $cfgfile 2>/dev/null
	    rm -rf $data_dir 2>/dev/null
	else
	    chmod 444 $cfgfile
	fi
    fi
    echo `date "+%T"` "File not saved; exiting" >&2
    exit 99
fi

#insert this run into the database
if [ "$program_type" = "custom" ] ; then
    echo $comment
    $info_program $datafile
else
    $info_program -m $comment --answer y $datafile
fi

#run laserrun if it is darkbox or laser type
if [ "$program_type" = "laser" ] || [ "$program_type" = "darkbox" ] ; then
    #laserrun takes run number argument
    ./laserrun -p ${filenamebase:3}
fi

#only if everything else works, empty out /incoming
#rm -f $temp_dir/Run*

exit 0
