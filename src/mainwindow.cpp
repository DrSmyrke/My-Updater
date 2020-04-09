#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QProcess>
#include <QTextBlock>

MainWindow::MainWindow(QWidget *parent) :
	QMainWindow(parent),
	ui(new Ui::MainWindow)
{
	ui->setupUi(this);

	m_pTimer = new QTimer();
	m_pManager = new QNetworkAccessManager( this );

	this->setWindowTitle( "My Updater v" + app::conf.version );
	this->setWindowIcon( QIcon( "://index.ico" ) );

	ui->addressBox->setText( app::conf.repository );
	ui->targetBox->setText( app::conf.targetDir );

	connect( ui->updateB, &QPushButton::clicked, this, &MainWindow::slot_update );
	connect( ui->addressBox, &QLineEdit::returnPressed, this, &MainWindow::slot_update );
	connect( m_pTimer, &QTimer::timeout, this, &MainWindow::slot_run );
	connect( ui->targetSB, &QPushButton::clicked, this, &MainWindow::slot_selectTarget );
	connect( ui->targetBox, &QLineEdit::returnPressed, this, &MainWindow::slot_selectTarget );

	m_pTimer->setInterval( 500 );
	m_state = State::downloadList;
	m_working = false;
	m_updatingF = false;
	ui->progressBar->setValue( 0 );
	//TODO: remove?
	m_applicationPath = QCoreApplication::applicationDirPath();
	ui->statusL->setText( " " );
}

MainWindow::~MainWindow()
{
	delete ui;
}

void MainWindow::slot_downloadProgress(const qint64 bytesReceived, const qint64 bytesTotal)
{
	auto str = QString( "   Downloading [%1/%2]...\n" ).arg( mf::getSize( (uint64_t)bytesReceived ) ).arg( mf::getSize( (uint64_t)bytesTotal ) );
	ui->statusL->setText( str );
}

void MainWindow::slot_readyRead()
{
	if( m_file.isOpen() && m_file.isWritable() ){
		m_file.write( m_pReply->readAll() );
	}else{
		m_buff.append( m_pReply->readAll() );
	}
}

void MainWindow::slot_finished()
{
	disconnect( m_pReply, &QNetworkReply::downloadProgress, this, &MainWindow::slot_downloadProgress );
	disconnect( m_pReply, &QNetworkReply::readyRead, this, &MainWindow::slot_readyRead );
	disconnect( m_pReply, &QNetworkReply::finished, this, &MainWindow::slot_finished );

	if( m_file.isOpen() ){
		m_file.close();
	}

	if( m_updatingF ){
		downloadUpdates();
		return;
	}

	m_working = false;
}

void MainWindow::slot_run()
{
	if( m_working ) return;

	auto assetsListFile = QString( "http://%1/assets.list" ).arg( app::conf.repository );
	ui->statusL->setText( " " );

	switch( m_state++ ){
		case State::downloadList:
			ui->progressBar->setValue( 20 );
			m_updatingF = false;
			m_downloadList.clear();
			ui->logBox->insertPlainText( tr( "Updating information from " ) );
			ui->logBox->insertPlainText( app::conf.repository );
			ui->logBox->insertPlainText( " ...\n" );
			startDownload( QUrl( assetsListFile ), "" );
		break;
		case State::decryptingList:
			ui->progressBar->setValue( 40 );
			ui->logBox->insertPlainText( tr( "Decrypting information...\n" ) );
			decryptList();
		break;
		case State::checkingFS:
			ui->progressBar->setValue( 60 );
			ui->logBox->insertPlainText( tr( "Checking filesystem...\n" ) );
			checkingFileSystem();
		break;
		case State::downloadUpdates:
			ui->progressBar->setValue( 80 );
			ui->logBox->insertPlainText( tr( "Download updates...\n" ) );
			downloadUpdates();
		break;
		case State::finished:
			ui->progressBar->setValue( 100 );
			if( m_pTimer->isActive() ){
				m_pTimer->start();
			}
			//TODO: Maybe?
			//emit close();
		break;
		default: break;
	}

	ui->logBox->moveCursor( QTextCursor::End );
}

void MainWindow::slot_selectTarget()
{
	auto target = QFileDialog::getExistingDirectory(this, tr("Open Target Directory"),
													app::conf.targetDir,
													QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
	ui->targetBox->setText( target );
}

void MainWindow::startDownload(const QUrl &url, const QString &fileName)
{
	m_working = true;
	m_buff.clear();

	if( !fileName.isEmpty() ){
		m_file.setFileName( fileName );
		m_file.open( QIODevice::WriteOnly );
	}

	m_pReply = m_pManager->get( QNetworkRequest( url ) );

	connect( m_pReply, &QNetworkReply::downloadProgress, this, &MainWindow::slot_downloadProgress );
	connect( m_pReply, &QNetworkReply::readyRead, this, &MainWindow::slot_readyRead );
	connect( m_pReply, &QNetworkReply::finished, this, &MainWindow::slot_finished );
}

void MainWindow::decryptList()
{
	m_working = true;
	m_updateList.clear();
	mf::XOR( m_buff, "magic_key" );
	m_updateList = QString( m_buff ).split( "\n" );
	m_working = false;
}

void MainWindow::checkingFileSystem()
{
	m_working = true;

	int i = 0;
	int totalFiles = m_updateList.size();

	for( auto elem:m_updateList ){
		auto tmp = elem.split( "\t" );
		if( tmp.size() < 3 ) continue;

		auto checkSumm = tmp[0];
		int fileSize = tmp[1].toInt();
		auto remoteFile = tmp[2];

		QString targetFile = QString( "%1/%2" ).arg( app::conf.targetDir ).arg( remoteFile );
		QFileInfo fi;
		fi.setFile( targetFile );
		auto targetDir = fi.absoluteDir().absolutePath();
		auto downloadFile = QString( "http://%1/%2" ).arg( app::conf.repository ).arg( remoteFile );
		if( !QDir( targetDir ).exists() ){
			QDir().mkpath( targetDir );
		}

		auto str = QString( tr("Checking files [%1/%2] ...\n") ).arg( i ).arg( totalFiles );
		ui->statusL->setText( str );
		i++;

		if( mf::checkFile( targetFile ) ){
			qint64 size = QFileInfo(targetFile).size();
			//struct stat stat_buf;
			//int rc = stat( targetFile.toUtf8().data(), &stat_buf);
			//qint64 size2 = (rc == 0) ? stat_buf.st_size : -1;
			if( size != fileSize ){
				QFile(targetFile).remove();
				addToUpdate( targetFile, downloadFile );
				continue;
			}
			if( checkSumm != mf::fileChecksum_MD5( targetFile ) ){
				QFile(targetFile).remove();
				addToUpdate( targetFile, downloadFile );
				continue;
			}
		}else{
			addToUpdate( targetFile, downloadFile );
		}
	}

	m_working = false;
}

void MainWindow::backSpace()
{
	while( ui->logBox->textCursor().block().text().length() > 0 ){
		ui->logBox->textCursor().deletePreviousChar();
	}
}

void MainWindow::downloadUpdates()
{
	m_working = true;

	if( m_downloadList.size() == 0 ){
		m_working = false;
		m_updatingF = false;
		return;
	}

	m_updatingF = true;

	drawProgress( m_downloadList.size() );

	auto data = m_downloadList.first();
	m_downloadList.pop_front();
	startDownload( QUrl( data.remoteFile ), data.localFile );
}

void MainWindow::drawProgress(const uint32_t filesLeft)
{
	auto str = QString( tr("   Files left: %1      ") ).arg( filesLeft );
	backSpace();
	ui->logBox->insertPlainText( str );
}

void MainWindow::addToUpdate(const QString &localFile, const QString &remoteFile)
{
	UpdateFileData data;
	data.localFile = localFile;
	data.remoteFile = remoteFile;
	m_downloadList.push_back( data );
}

void MainWindow::slot_update()
{
	auto repo = ui->addressBox->text();
	auto target = ui->targetBox->text();
	if( repo.isEmpty() && target.isEmpty() ){
		return;
	}

	if( !QDir( app::conf.targetDir ).exists() ){
		QDir().mkpath( app::conf.targetDir );
	}
	if( !QDir( app::conf.targetDir ).exists() ){
		ui->logBox->insertPlainText( tr( "Target dir not found\n" ) );
		return;
	}

	app::conf.repository = repo;
	app::conf.targetDir = target;
	m_pTimer->start();
}

//void MainWindow::slot_start()
//{
//	auto login = ui->loginBox->text();
//	auto javaPath = ui->javaBox->text();

//	if( login.isEmpty() || javaPath.isEmpty() ){
//		return;
//	}

//	app::conf.login = login;
//	app::conf.javaPath = javaPath;
//	app::saveSettings();

//	auto appPath = QCoreApplication::applicationDirPath();

//	QStringList args;

//	args.append( "-Dfml.ignoreInvalidMinecraftCertificates=true" );
//	args.append( "-Xincgc" );
//	args.append( "-Xmx1024M" );
//	args.append( "-Xmn128M" );
//	args.append( QString( "-Djava.library.path=\"%1\\versions\\Forge 1.6.4\\natives\"" ).arg( appPath ) );
//	args.append( "-cp" );

//	QStringList libs;

//	scanDir( QString( "%1/libraries" ).arg( appPath ), libs );
//	scanDir( QString( "%1/versions" ).arg( appPath ), libs );

//	args.append( QString( "\"%1\"" ).arg( libs.join(";") ) );
//	args.append( "net.minecraft.launchwrapper.Launch" );
//	args.append( "--username" );
//	args.append( login );
//	args.append( "--session" );
//	args.append( "%random%" );
//	args.append( "--uuid" );
//	args.append( "\"%random%%random%-%random%-%random%-%random%-%random%%random%\"" );
//	args.append( "--accessToken" );
//	args.append( "\"%random%%random%-%random%-%random%-%random%-%random%%random%\"" );
//	args.append( "--version" );
//	args.append( "\"Forge 1.6.4\"" );
//	args.append( "--gameDir" );
//	args.append( QString( "\"%1\"" ).arg( appPath ) );
//	args.append( "--assetsDir" );
//	args.append( QString( "\"%1/assets\"" ).arg( appPath ) );
//	args.append( "--tweakClass" );
//	args.append( "cpw.mods.fml.common.launcher.FMLTweaker" );

//	QFile f( "start.bat" );
//	if( f.open( QIODevice::WriteOnly ) ){
//		f.write( QString( "\"%1\"" ).arg( javaPath ).toUtf8() );
//		f.write( " " );
//		f.write( args.join( " " ).toUtf8() );
//		f.close();
//	}

//	QProcess cmd;
//	cmd.startDetached("cmd",QStringList()<<"/C"<<"start.bat");

//	emit close();
//}
