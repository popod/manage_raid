//============================================================================
// Name        : main.cpp
// Author      : Maxime Letourneau
// Version     : 1.0
// Description : Raid manager for mdadm
//
// Language	   : C++ 2011
//============================================================================

#include <string>
#include <vector>
#include <algorithm>
#include <cctype>

#include "Raid.h"
#include "spdlog/spdlog.h"
#include "configuration/configuration.h"

using namespace std;

int main(int argc, char*argv[]) {
	vector<string> 	options;
	Configuration	config;
	string 			raidDisk, disk, state, path;
	string			raidName, raidMount, logFile, logLevel; 	// for config file
	int				logRotOn, logSize, logMaxFile;		// for config file
					/* logSize in Mo
					 *
					 */
	int 			i;

	shared_ptr<spdlog::logger> log;	// var pointer for the log lib

	path = argv[0];
	path.erase(path.end() - (path.length() - path.find_last_of("/")) + 1, path.end());

	// check that the program is launching by root
	if(getuid() != 0) {
		cerr << "Error : must be launch by root" << endl;
		return EXIT_FAILURE;
	}

	config.Load(path + "raid.conf");	// load the config file !!! ajouter gestion erreur ouverture fichier !!!
	if (!(config.Get("RAID_NAME", 		raidName)		&&		// read essential parameter of config file
		  config.Get("RAID_MOUNT", 		raidMount)		&&
		  config.Get("LOG_ROTATE_ON", 	logRotOn)		&&
		  config.Get("LOG_FILE", 		logFile)		&&
		  config.Get("LOG_SIZE", 		logSize)		&&
		  config.Get("LOG_MAX_FILE",	logMaxFile)		&&
		  config.Get("LOG_LEVEL",		logLevel)))	{

		cerr << "Error : missing parameter in configuration file." << endl;
		return EXIT_FAILURE;
	}

	Raid md0(raidName, raidMount, path);

	// choose log method : with or not a rotation of the log
	if(logRotOn) log = spdlog::rotating_logger_st("manage_raid", logFile, 1024 * 1024 * logSize, logMaxFile);
	else log = spdlog::basic_logger_st("manage_raid", logFile);

	// log level
	// set default log level
	spdlog::set_level(spdlog::level::info);
	// set log level read in config file
	for(i=0; i<spdlog::level::off; i++){
		if(logLevel == spdlog::level::to_str((spdlog::level::level_enum) i)) spdlog::set_level((spdlog::level::level_enum)i);
	}

	log->info("manage raid has been launch");

	for(i=0;i<argc;i++) options.push_back(argv[i]);

	if(argc>1){
		if(options[1] == "--tagflow"){
			for(i=1;i<argc;i++){
				if(options[i] == "--size"){
					log->info("ask disk raid space");
					int Aspace, Tspace;
					md0.statMem(Aspace, Tspace);
					log->info(" disk raid space : available space = {} GB       total space = {} GB", Aspace, Tspace);

					/*cout << "Space state :" << endl;		// uncomment to have information in stdout
					cout << "available space : " << Aspace << "GB" << endl;
					cout << "total space     : " << Tspace << "GB" << endl;*/
				}

				if(options[i] == "--reconstruction-state"){
					log->info("ask reconstruction state");
					double recovery, finish, speed;
					md0.rebuildState(recovery, finish, speed);
					log->info("reconstruction state : rebuild state = {} %       time remaining = {} min       speed = {} Mo/s", recovery, finish, speed);

					/*cout << "Reconstruction state :" << endl; 	// uncomment to have information in stdout
					cout << "rebuild state = " << recovery << " %" << endl;
					cout << "time remaining = " << finish << " min" << endl;
					cout << "speed = " << speed << " Mo/s" << endl;*/
				}
			}
		}
		else{
			raidDisk 	= options[2];

			transform(options[1].begin(), options[1].end(), options[1].begin(), ::tolower);

			if(options[1].find("fail") != string::npos){
				disk		= options[3];

				log->info("Disk {} fail", disk);

				log->info("starting removing of the disk in the raid array");
				if(md0.diskManipulation(disk, "remove")) log->info("removing disk fail");
				else log->info("removing disk done");

				log->info("starting the smart test");
				if(md0.smartTest(disk, state)) log->info("smart test fail");
				else log->info("smart test done");

				if(state == ""){	// not defect disk
					log->info("starting formatting disk");
					if(md0.diskManipulation(disk, "format")) log->info("format disk fail");
					else log->info("format disk done");

					log->info("starting adding disk in the raid array");
					if(md0.diskManipulation(disk, "add")) log->info("adding disk fail");
					else log->info("adding disk done");
				}
				else{				// defect disk
					log->info("Disk {} is defect. Please change the disk", disk);

					do{				// wait that the disk is change
						 sleep(30);
					}while(md0.diskDetection(disk));
					log->info("New disk detected");

					log->info("starting formatting disk");
					if(md0.diskManipulation(disk, "format")) log->info("format disk fail");
					else log->info("format disk done");

					log->info("starting adding disk in the raid array");
					if(md0.diskManipulation(disk, "add")) log->info("adding disk fail");
					else log->info("adding disk done");
				}
			}
			if(options[1] == "rebuildstarted"){
				log->info("Rebuild of {} started", disk);
			}

			if(options[1] == "rebuildfinished"){
				disk		= options[3];
				log->info("Rebuild of {} finished", disk);
			}

		}
		return EXIT_SUCCESS;
	}
	else{
		cerr << "Error : need argument" << endl;;
		log->info("Error launching program : need argument");
		return EXIT_FAILURE;
	}


}

