#ifndef GLOBAL_H
#define GLOBAL_H

#include <QString>

struct Config{
	QString version;
	QString repository;
	QString targetDir;
};

namespace app {
	extern Config conf;

	bool parsArgs(int argc, char *argv[]);
	void setLog(const uint8_t logLevel, const QString &mess);
}

#endif // GLOBAL_H
