#include "global.h"

#include <QSettings>

namespace app {
	Config conf;

	bool parsArgs(int argc, char *argv[])
	{
		bool ret = true;
		for(int i=0;i<argc;i++){
			if(QString(argv[i]).indexOf("-")==0){
				if(QString(argv[i]) == "--help" or QString(argv[1]) == "-h"){
					printf("Usage: %s [OPTIONS]\n"
							"  -l	login\n"
							"  -v    Verbose output\n"
							"\n", argv[0]);
					ret = false;
				}
				//if(QString(argv[i]) == "-l") app::conf.login = QString(argv[++i]);
				//if(QString(argv[i]) == "-v") app::conf.verbose = true;
			}
		}
		return ret;
	}

	void setLog(const uint8_t logLevel, const QString &mess)
	{
		Q_UNUSED( logLevel )
		Q_UNUSED( mess )
		return;
	}

}
