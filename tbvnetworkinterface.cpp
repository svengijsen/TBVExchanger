//Copyright (C) 2015  Michael Luehrs, Brain Innovation B.V.
//
//This file is part of BrainStim.
//BrainStim is free software: you can redistribute it and/or modify
//it under the terms of the GNU General Public License as published by
//the Free Software Foundation, either version 3 of the License, or
//(at your option) any later version.
//
//This program is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//GNU General Public License for more details.
//
//You should have received a copy of the GNU General Public License
//along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

#include "tbvnetworkinterface.h"

TBVNetworkInterface::TBVNetworkInterface(bool autoConnect, bool autoReconnect) : QThread()
{

	VMajor = 1;
	VMinor = 6;
	VSubminor = 0;

	tcpSocket = new QTcpSocket();
	tcpSocket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
	tcpSocket->setSocketOption(QAbstractSocket::KeepAliveOption, 1);

	rTcpSocket = new QTcpSocket();
	rTcpSocket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
	rTcpSocket->setSocketOption(QAbstractSocket::KeepAliveOption, 1);

	nDefaultPort = (quint16)55555;
	blockSize = 0;
	terminateReconnect = false;
	terminateReciever = false;
	manualDisconnect = false;
	udpAutoConnect = autoConnect;
	reConnect = autoReconnect;
	udpSocket = NULL;

	connect(tcpSocket,SIGNAL(connected()),this,SLOT(connectionEstablished()));
	connect(rTcpSocket,SIGNAL(disconnected()),this,SLOT(connectionLost()));
	connect(tcpSocket,SIGNAL(error(QAbstractSocket::SocketError)),this,SLOT(writeError(QAbstractSocket::SocketError)));

	querryQueue = new QQueue<QString>;
		
	if(udpAutoConnect)
		setAutoConnection(true);
}

TBVNetworkInterface::~TBVNetworkInterface()
{
	terminateReconnect = true;
	terminateReciever = true;

	if(tcpSocket)
	{
		delete tcpSocket;
		tcpSocket = NULL;
	}
	if(rTcpSocket)
	{
		delete rTcpSocket;
		rTcpSocket = NULL;
	}
	if(querryQueue)
	{
		delete querryQueue;
		querryQueue = NULL;
	}
	if(udpSocket)
	{
		delete udpSocket;
		udpSocket = NULL;
	}
}


bool TBVNetworkInterface::isConnected()
{
	return tcpSocket->state()==QAbstractSocket::ConnectedState;
}

void TBVNetworkInterface::processPendingDatagrams()
{
    while (udpSocket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(udpSocket->pendingDatagramSize());
		QHostAddress *tbvServer = new QHostAddress();
		quint16 *tbvPort = new quint16();
		udpSocket->readDatagram(datagram.data(),datagram.size(),tbvServer,tbvPort);

		quint16 port = tr(datagram.data()).remove("Turbo-BrainVoyager Broadcast: ").toInt();


		if(tr(datagram.data()).contains(tr("Turbo-BrainVoyager Broadcast")))
		{
			if(!connectToServer(tbvServer->toString().toLatin1().data(),port))
			{
				udpSocket->close();
				writeError(tcpSocket->error());
			}
			else
			{
				reConnect = false;
			}
			return;
		}
    }
}

void TBVNetworkInterface::connectionEstablished()
{
	if(udpSocket)
		udpSocket->close();
	emit connected(ipAddressString,iPort);
}

void TBVNetworkInterface::connectionLost()
{
	emit disconnected();
	if(manualDisconnect)
		return;
	if(reConnect)
		tryToReconnect();
	else if(udpAutoConnect)
		setAutoConnection(true);
}

void TBVNetworkInterface::tryToReconnect()
{
	if (udpSocket)
		udpSocket->close();
	if (tcpSocket)
	{
		tcpSocket->readAll();
		tcpSocket->close();
	}
	if (rTcpSocket)
	{
		rTcpSocket->readAll();
		rTcpSocket->close();
	}

	disconnect(rTcpSocket, SIGNAL(readyRead()), this, SLOT(readExecuteStep()));

	QTime *timer = new QTime();
	emit connecting();
	while (!terminateReconnect && !connectToServer(ipAddressString.toLatin1().data(), iPort))
	{
		timer->restart();
		while(timer->elapsed()<2000)
		{
			QCoreApplication::processEvents();
			msleep(1);
		}
	}
}
bool TBVNetworkInterface::connectToServer(char *ipAddress,quint16 port)
{
	manualDisconnect = false;
	ipAddressString = ipAddress;
	iPort = port;
	tcpSocket->connectToHost(tr(ipAddress),port);
	if(!tcpSocket->waitForConnected(2000))
		return false;

	sendStreamDefinition("Request Socket",tcpSocket);
	if(!checkConnection("Request Socket",tcpSocket))
	{
		tcpSocket->close();
		return false;
	}

	msleep(100);
	rTcpSocket->connectToHost(tr(ipAddress),port);
	if(!rTcpSocket->waitForConnected(2000))
	{
		tcpSocket->close();
		return false;
	}
	sendStreamDefinition("Execute Socket",rTcpSocket);
	if(!checkConnection("Execute Socket",rTcpSocket))
	{
		rTcpSocket->close();
		tcpSocket->close();
		return false;
	}
	connect(rTcpSocket,SIGNAL(readyRead()),this,SLOT(readExecuteStep()));

	return true;
}


bool TBVNetworkInterface::sendStreamDefinition(char *definition,QTcpSocket *dSocket)
{
	QByteArray definit;
	QDataStream out(&definit, QIODevice::WriteOnly);
	out.setVersion(QDataStream::Qt_4_8);
	out.setFloatingPointPrecision(QDataStream::SinglePrecision);
	out << (qint64)0;
	out << definition;	
	out.device()->seek(0);
	out << (qint64)(definit.size() - sizeof(qint64));

	if(dSocket->write(definit) == 0)
	{
		if(dSocket->state() != QAbstractSocket::ConnectedState)
			return false;
	}
	
	if(!dSocket->waitForBytesWritten())
	{
		return false;
	}

	
	return true;

}

bool TBVNetworkInterface::checkConnection(QString socketName, QTcpSocket *dSocket)
{
	while(!terminateReciever)
	{
		QDataStream in(dSocket);
		in.setVersion(QDataStream::Qt_4_8);
		in.setFloatingPointPrecision(QDataStream::SinglePrecision);
		dSocket->waitForReadyRead(1000);
		//get size of data to receive
		if (blockSize == 0) {
			if (dSocket->bytesAvailable() < (int)sizeof(qint64))
			{
				return false;
			}
			in >> blockSize;
		}
		//ensure that enough data is on the stream
		if ((qint64)dSocket->bytesAvailable() < blockSize)
		{
			return false;
		}

		blockSize = 0;

		char *rcvSocketName;
		in >> rcvSocketName;
		int major, minor, subminor;
		in >> major;
		in >> minor;
		in >> subminor;

		if(!rcvSocketName)
		{
			while (!dSocket->atEnd()) 
			{
				QByteArray data = dSocket->read(100);
			}
		}


		if(tr(rcvSocketName) == socketName && major == VMajor && minor >= VMinor)
			return true;
		else
			return false;

	}
	return false;
}

bool TBVNetworkInterface::disconnectFromServer()
{	
	manualDisconnect = true;
	udpSocket->close();
	tcpSocket->close();
	rTcpSocket->close();
	return true;
}

bool TBVNetworkInterface::setAutoConnection(bool setting)
{
	if(setting)
	{
		if(udpSocket == NULL)
			udpSocket = new QUdpSocket();

		bool bBindResult = udpSocket->bind(nDefaultPort, QUdpSocket::ShareAddress);
		bool bConnectResult = connect(udpSocket, SIGNAL(readyRead()),this, SLOT(processPendingDatagrams()));
		return bConnectResult && bBindResult;
	}
	else
	{
		if(udpSocket)
		{
			disconnect(udpSocket, SIGNAL(readyRead()),this, SLOT(processPendingDatagrams()));
			udpSocket->close();
			delete udpSocket;
			udpSocket = NULL;
		}
		return false;
	}
}

void TBVNetworkInterface::readExecuteStep()
{
	while(!terminateReciever)
	{
		QDataStream in(rTcpSocket);
		in.setVersion(QDataStream::Qt_4_8);
		in.setFloatingPointPrecision(QDataStream::SinglePrecision);
		//get size of data to receive
		if (blockSize == 0) {
			if (rTcpSocket->bytesAvailable() < (int)sizeof(qint64))
			{
				return;
			}
			in >> blockSize;
		}
		//ensure that enough data is on the stream
		if ((qint64)rTcpSocket->bytesAvailable() < blockSize)
		{
			return;
		}

		blockSize = 0;
		char *rcvChar;
		int timepoint;
		in >> rcvChar;

		if(!rcvChar)
		{
			while (!rTcpSocket->atEnd()) 
			{
				QByteArray data = rTcpSocket->read(100);
			}
		}
		else
		if(strcmp(rcvChar , "PreStepCalled") == 0)
		{
			in >> timepoint;
			emit executePreStep(timepoint);
		}
		else
		if(strcmp(rcvChar , "PostStepCalled") == 0)
		{
			in >> timepoint;
			emit executePostStep(timepoint);	
		}
		else
		if(strcmp(rcvChar , "PostRunCalled") == 0)
		{
			in >> timepoint;
			emit executePostRun(timepoint);
		}
		else
		while (!rTcpSocket->atEnd()) 
		{
			  QByteArray data = tcpSocket->read(100);
		}
	}
}

//Basic TBV Project Functions
int TBVNetworkInterface::tGetCurrentTimePoint()
{
	TBV_Server_Request doRequest(tcpSocket,querryQueue,tr("tGetCurrentTimePoint"));
	doRequest.sendData("tGetCurrentTimePoint");
	QString status = doRequest.getReply();
	
	if(!status.isEmpty())
	{
		return 0;
	}
	int CurrentTimePoint;
	doRequest.getDataOfByteArray(CurrentTimePoint);
	return CurrentTimePoint;
}
int	TBVNetworkInterface::tGetExpectedNrOfTimePoints()
{
	TBV_Server_Request doRequest(tcpSocket,querryQueue,tr("tGetExpectedNrOfTimePoints"));
	doRequest.sendData("tGetExpectedNrOfTimePoints");
	QString status = doRequest.getReply();

	if(!status.isEmpty())
	{
		return 0;
	}
	int ExpectedNrOfTimePoints;
	doRequest.getDataOfByteArray(ExpectedNrOfTimePoints);
	return ExpectedNrOfTimePoints;
}
QList<int> TBVNetworkInterface::tGetDimsOfFunctionalData()
{
	int dim_x,dim_y,dim_z;
	TBV_Server_Request doRequest(tcpSocket,querryQueue,tr("tGetDimsOfFunctionalData"));
	doRequest.sendData("tGetDimsOfFunctionalData");
	QString status = doRequest.getReply();
	QList<int> dimsofFunctionalData;
	if(!status.isEmpty())
	{
		dimsofFunctionalData.clear();
		dimsofFunctionalData.append(0);
		return dimsofFunctionalData;
	}
	doRequest.getDataOfByteArray(dim_x);
	doRequest.getDataOfByteArray(dim_y);
	doRequest.getDataOfByteArray(dim_z);

	
	dimsofFunctionalData.append(dim_x);
	dimsofFunctionalData.append(dim_y);
	dimsofFunctionalData.append(dim_z);
	return dimsofFunctionalData;
}
QString TBVNetworkInterface::tGetProjectName()
{
	TBV_Server_Request doRequest(tcpSocket,querryQueue,tr("tGetProjectName"));
	doRequest.sendData("tGetProjectName");
	QString status = doRequest.getReply();

	if(!status.isEmpty())
	{
		return "";
	}
	char *ProjectName;
	doRequest.getDataOfByteArray(ProjectName);
	return QString(ProjectName);
}
QString TBVNetworkInterface::tGetWatchFolder()
{
	TBV_Server_Request doRequest(tcpSocket,querryQueue,tr("tGetWatchFolder"));
	doRequest.sendData("tGetWatchFolder");
	QString status = doRequest.getReply();

	if(!status.isEmpty())
	{
		return "";
	}
	char *WatchFolder;
	doRequest.getDataOfByteArray(WatchFolder);
	return QString(WatchFolder);
}
QString TBVNetworkInterface::tGetTargetFolder()
{
	TBV_Server_Request doRequest(tcpSocket,querryQueue,tr("tGetTargetFolder"));
	doRequest.sendData("tGetTargetFolder");
	QString status = doRequest.getReply();

	if(!status.isEmpty())
	{
		return "";
	}
	char *TargetFolder;
	doRequest.getDataOfByteArray(TargetFolder);
	return QString(TargetFolder);
}
QString TBVNetworkInterface::tGetFeedbackFolder()
{
	TBV_Server_Request doRequest(tcpSocket,querryQueue,tr("tGetFeedbackFolder"));
	doRequest.sendData("tGetFeedbackFolder");
	QString status = doRequest.getReply();

	if(!status.isEmpty())
	{
		return "";
	}
	char *FeedbackFolder;
	doRequest.getDataOfByteArray(FeedbackFolder);
	return QString(FeedbackFolder);
}

//TBV Protocol, DM, GLM Functions:
int TBVNetworkInterface::tGetCurrentProtocolCondition()
{
	TBV_Server_Request doRequest(tcpSocket,querryQueue,tr("tGetCurrentProtocolCondition"));
	doRequest.sendData("tGetCurrentProtocolCondition");
	QString status = doRequest.getReply();
	
	if(!status.isEmpty())
	{
		return 0;
	}
	int CurrentProtocolCondition;
	doRequest.getDataOfByteArray(CurrentProtocolCondition);
	return CurrentProtocolCondition;
}
int	TBVNetworkInterface::tGetFullNrOfPredictors()
{
	TBV_Server_Request doRequest(tcpSocket,querryQueue,tr("tGetFullNrOfPredictors"));
	doRequest.sendData("tGetFullNrOfPredictors");
	QString status = doRequest.getReply();

	if(!status.isEmpty())
	{
		return 0;
	}
	int FullNrOfPredictors;
	doRequest.getDataOfByteArray(FullNrOfPredictors);
	return FullNrOfPredictors;
}
int TBVNetworkInterface::tGetCurrentNrOfPredictors()
{
	TBV_Server_Request doRequest(tcpSocket,querryQueue,tr("tGetCurrentNrOfPredictors"));
	doRequest.sendData("tGetCurrentNrOfPredictors");
	QString status = doRequest.getReply();
	
	if(!status.isEmpty())
	{
		return 0;
	}
	int CurrentNrOfPredictors;
	doRequest.getDataOfByteArray(CurrentNrOfPredictors);
	return CurrentNrOfPredictors;
}
int TBVNetworkInterface::tGetNrOfConfoundPredictors()
{
	TBV_Server_Request doRequest(tcpSocket,querryQueue,tr("tGetNrOfConfoundPredictors"));
	doRequest.sendData("tGetNrOfConfoundPredictors");
	QString status = doRequest.getReply();

	if(!status.isEmpty())
	{
		return 0;
	}
	int NrOfConfoundPredictors;
	doRequest.getDataOfByteArray(NrOfConfoundPredictors);
	return NrOfConfoundPredictors;
}
float TBVNetworkInterface::tGetValueOfDesignMatrix(int pred, int timepoint)
{
	TBV_Server_Request doRequest(tcpSocket,querryQueue,tr("tGetValueOfDesignMatrix"));
	doRequest.sendData("tGetValueOfDesignMatrix", pred, timepoint);
	QString status = doRequest.getReply();
	
	if(!status.isEmpty())
	{
		return 0;
	}
	float ValueOfDesignMatrix;
	doRequest.getDataOfByteArray(ValueOfDesignMatrix);
	return ValueOfDesignMatrix;
}
int TBVNetworkInterface::tGetNrOfContrasts()
{
	TBV_Server_Request doRequest(tcpSocket,querryQueue,tr("tGetNrOfContrasts"));
	doRequest.sendData("tGetNrOfContrasts");
	QString status = doRequest.getReply();
	
	if(!status.isEmpty())
	{
		return 0;
	}
	int NrOfContrasts;
	doRequest.getDataOfByteArray(NrOfContrasts);
	return NrOfContrasts;
}


//TBV ROI Functions
int TBVNetworkInterface::tGetNrOfROIs()
{
	TBV_Server_Request doRequest(tcpSocket,querryQueue,tr("tGetNrOfROIs"));
	doRequest.sendData("tGetNrOfROIs");	
	QString status = doRequest.getReply();
	
	if(!status.isEmpty())
	{
		return 0;
	}
	int NrOfROIs;
	doRequest.getDataOfByteArray(NrOfROIs);
	return NrOfROIs;
}
float TBVNetworkInterface::tGetMeanOfROI(int roi)
{	
	TBV_Server_Request doRequest(tcpSocket,querryQueue,tr("tGetMeanOfROI"));
	doRequest.sendData("tGetMeanOfROI",roi);
	QString status = doRequest.getReply();
	
	if(!status.isEmpty())
	{
		return 0;
	}
	float MeanOfROI;
	doRequest.getDataOfByteArray(MeanOfROI);
	return MeanOfROI;
}
QList<float> TBVNetworkInterface::tGetExistingMeansOfROI(int roi, int toTimePoint)
{	
	TBV_Server_Request doRequest(tcpSocket,querryQueue,tr("tGetExistingMeansOfROI"));
	doRequest.sendData("tGetExistingMeansOfROI",roi,toTimePoint);
	QString status = doRequest.getReply();

	if(!status.isEmpty())
	{
		AllMeansOfROI.clear();
		AllMeansOfROI.append(0);
		return AllMeansOfROI.toList();
	}

	AllMeansOfROI.resize(toTimePoint);
	doRequest.getVectorDataOfByteArray(&AllMeansOfROI,toTimePoint);
	AllMeansOfROI.append(1);
	return AllMeansOfROI.toList();

}
float TBVNetworkInterface::tGetMeanOfROIAtTimePoint(int roi,int timepoint)
{	
	TBV_Server_Request doRequest(tcpSocket,querryQueue,tr("tGetMeanOfROIAtTimePoint"));
	doRequest.sendData("tGetMeanOfROIAtTimePoint",roi,timepoint);
	QString status = doRequest.getReply();

	if(!status.isEmpty())
	{
		return 0;
	}
	float MeanOfROI;
	doRequest.getDataOfByteArray(MeanOfROI);
	return MeanOfROI;
}
int	TBVNetworkInterface::tGetNrOfVoxelsOfROI(int roi)
{
	TBV_Server_Request doRequest(tcpSocket,querryQueue,tr("tGetNrOfVoxelsOfROI"));
	doRequest.sendData("tGetNrOfVoxelsOfROI",roi);
	QString status = doRequest.getReply();
	
	if(!status.isEmpty())
	{
		return 0;
	}
	int NrOfVoxelsOfROI;
	doRequest.getDataOfByteArray(NrOfVoxelsOfROI);
	return NrOfVoxelsOfROI;
}
float TBVNetworkInterface::tGetBetaOfROI(int roi,int beta)
{
	TBV_Server_Request doRequest(tcpSocket,querryQueue,tr("tGetBetaOfROI"));
	doRequest.sendData("tGetBetaOfROI",roi,beta);
	QString status = doRequest.getReply();
	
	if(!status.isEmpty())
	{
		return 0;
	}
	float BetaOfROI;
	doRequest.getDataOfByteArray(BetaOfROI);
	return BetaOfROI;

}
QList<int> TBVNetworkInterface::tGetCoordsOfVoxelOfROI(int roi, int voxel)
{
	int x,y,z;
	TBV_Server_Request doRequest(tcpSocket,querryQueue,tr("tGetCoordsOfVoxelOfROI"));
	doRequest.sendData("tGetCoordsOfVoxelOfROI",roi,voxel);
	QString status = doRequest.getReply();
	QList<int> coords;
	if(!status.isEmpty())
	{
		coords.clear();
		coords.append(0);
		return coords;
	}
	
	doRequest.getDataOfByteArray(x);
	doRequest.getDataOfByteArray(y);
	doRequest.getDataOfByteArray(z);
	coords.append(x);
	coords.append(y);
	coords.append(z);
	coords.append(1);
	return coords;
}
QList<int> TBVNetworkInterface::tGetAllCoordsOfVoxelsOfROI(int roi)
{
	int NrOfVoxelsOfROI = tGetNrOfVoxelsOfROI(roi);

	TBV_Server_Request doRequest(tcpSocket,querryQueue,tr("tGetAllCoordsOfVoxelsOfROI"));
	doRequest.sendData("tGetAllCoordsOfVoxelsOfROI",roi);
	QString status = doRequest.getReply();
	
	if(!status.isEmpty())
	{
		CoordsOfVoxelsOfROI.clear();
		CoordsOfVoxelsOfROI.append(0);
		return CoordsOfVoxelsOfROI.toList();
	}

	
	CoordsOfVoxelsOfROI.resize(NrOfVoxelsOfROI*3);

	doRequest.getVectorDataOfByteArray(&CoordsOfVoxelsOfROI,NrOfVoxelsOfROI*3);
	CoordsOfVoxelsOfROI.append(1);
	return CoordsOfVoxelsOfROI.toList();
}


//TBV Volume Data Access Functions
float TBVNetworkInterface::tGetValueOfVoxelAtTime(int x, int y, int z, int timepoint)
{
	TBV_Server_Request doRequest(tcpSocket,querryQueue,tr("tGetValueOfVoxelAtTime"));
	doRequest.sendData("tGetValueOfVoxelAtTime",x,y,z,timepoint);
	QString status = doRequest.getReply();

	if(!status.isEmpty())
	{
		return false;
	}
	float ValueOfVoxelAtTime;
	doRequest.getDataOfByteArray(ValueOfVoxelAtTime);
	return ValueOfVoxelAtTime;
}
QList<short> TBVNetworkInterface::tGetValueOfAllVoxelsAtTime(int timepoint)
{
	int dim_xyz;
	QList<int> dimsOfFunctionalData = tGetDimsOfFunctionalData();

	dim_xyz = dimsOfFunctionalData.at(0) * dimsOfFunctionalData.at(1) * dimsOfFunctionalData.at(2);
		
	TBV_Server_Request doRequest(tcpSocket,querryQueue,tr("tGetValueOfAllVoxelsAtTime"));
	doRequest.sendData("tGetValueOfAllVoxelsAtTime", timepoint);
	QString status = doRequest.getReply();
	
	if(!status.isEmpty())
	{
		TimeCourseData.clear();
		TimeCourseData.append(0);
		return TimeCourseData.toList();
	}
	
	TimeCourseData.resize(dim_xyz);
	doRequest.getVectorDataOfByteArray(&TimeCourseData,dim_xyz);
	TimeCourseData.append(1);
	return TimeCourseData.toList();
}
QList<short> TBVNetworkInterface::tGetRawValueOfAllVoxelsAtTime(int timepoint)
{
	int dim_xyz;
	QList<int> dimsOfFunctionalData = tGetDimsOfFunctionalData();

	dim_xyz = dimsOfFunctionalData.at(0) * dimsOfFunctionalData.at(1) * dimsOfFunctionalData.at(2);

	TBV_Server_Request doRequest(tcpSocket,querryQueue,tr("tGetRawValueOfAllVoxelsAtTime"));
	doRequest.sendData("tGetRawValueOfAllVoxelsAtTime", timepoint);
	QString status = doRequest.getReply();
	
	if(!status.isEmpty())
	{
		RawTimeCourseData.clear();
		RawTimeCourseData.append(0);
		return RawTimeCourseData.toList();
	}
	RawTimeCourseData.resize(dim_xyz);
	doRequest.getVectorDataOfByteArray(&RawTimeCourseData,dim_xyz);
	RawTimeCourseData.append(1);
	return RawTimeCourseData.toList();
}
double TBVNetworkInterface::tGetBetaOfVoxel(int beta, int x, int y, int z)
{
	TBV_Server_Request doRequest(tcpSocket,querryQueue,tr("tGetBetaOfVoxel"));
	doRequest.sendData("tGetBetaOfVoxel", beta, x, y, z);
	QString status = doRequest.getReply();
	
	if(!status.isEmpty())
	{
		return 0;
	}
	double BetaOfVoxel;
	doRequest.getDataOfByteArray(BetaOfVoxel);
	return BetaOfVoxel;
}
QList<double> TBVNetworkInterface::tGetBetaMaps()
{
	int dim_xyz;
	QList<int> dimsOfFunctionalData = tGetDimsOfFunctionalData();
	dim_xyz = dimsOfFunctionalData.at(0) * dimsOfFunctionalData.at(1) * dimsOfFunctionalData.at(2);

	int n_predictors_current = tGetCurrentNrOfPredictors();
	
	TBV_Server_Request doRequest(tcpSocket,querryQueue,tr("tGetBetaMaps"));
	doRequest.sendData("tGetBetaMaps");
	QString status = doRequest.getReply();
	
	if(!status.isEmpty())
	{
		BetaMaps.clear();
		BetaMaps.append(0);
		return BetaMaps.toList();
	}
	BetaMaps.resize(n_predictors_current*dim_xyz);
	doRequest.getVectorDataOfByteArray(&BetaMaps, n_predictors_current*dim_xyz);
	BetaMaps.append(1);
	return BetaMaps.toList();
}
float TBVNetworkInterface::tGetMapValueOfVoxel(int map, int x, int y, int z)
{
	TBV_Server_Request doRequest(tcpSocket,querryQueue,tr("tGetMapValueOfVoxel"));
	doRequest.sendData("tGetMapValueOfVoxel", map, x,y,z);
	QString status = doRequest.getReply();
	
	if(!status.isEmpty())
	{
		return 0;
	}
	float MapValueOfVoxel;
	doRequest.getDataOfByteArray(MapValueOfVoxel);
	return MapValueOfVoxel;
}
QList<float> TBVNetworkInterface::tGetContrastMaps()
{
	int dim_xyz;
	QList<int> dimsOfFunctionalData = tGetDimsOfFunctionalData();
	dim_xyz = dimsOfFunctionalData.at(0) * dimsOfFunctionalData.at(1) * dimsOfFunctionalData.at(2);

	int n_contrast_maps = tGetNrOfContrasts();
	
	TBV_Server_Request doRequest(tcpSocket,querryQueue,tr("tGetContrastMaps"));
	doRequest.sendData("tGetContrastMaps");
	QString status = doRequest.getReply();
	
	if(!status.isEmpty())
	{
		ContrastMaps.clear();
		ContrastMaps.append(0);
		return ContrastMaps.toList();
	}
	ContrastMaps.resize(n_contrast_maps*dim_xyz*sizeof(float));
	doRequest.getVectorDataOfByteArray(&ContrastMaps, n_contrast_maps*dim_xyz);
	ContrastMaps.append(1);
	return ContrastMaps.toList();
}
int TBVNetworkInterface::tGetNumberOfClasses()
{	
	TBV_Server_Request doRequest(tcpSocket,querryQueue,tr("tGetNumberOfClasses"));
	doRequest.sendData("tGetNumberOfClasses");
	QString status = doRequest.getReply();

	if(!status.isEmpty())
	{
		return -1;
	}
	int NumberOfClasses;
	doRequest.getDataOfByteArray(NumberOfClasses);
	return NumberOfClasses;
}
QList<float> TBVNetworkInterface::tGetCurrentClassifierOutput()
{
	int n_classes = tGetNumberOfClasses();

	TBV_Server_Request doRequest(tcpSocket,querryQueue,tr("tGetCurrentClassifierOutput"));
	doRequest.sendData("tGetCurrentClassifierOutput");
	QString status = doRequest.getReply();

	if(!status.isEmpty())
	{
		ClassifierOutput.clear();
		ClassifierOutput.append(0);
		return ClassifierOutput.toList();
	}	
	int winner_class;
	doRequest.getDataOfByteArray(winner_class);

	int n_class_comparisons = n_classes * (n_classes - 1) / 2;
	ClassifierOutput.resize(n_class_comparisons);
	doRequest.getVectorDataOfByteArray(&outputVector, n_class_comparisons);
	ClassifierOutput.append(winner_class);
	ClassifierOutput.append(1);
	return ClassifierOutput.toList();
}

QList<double> TBVNetworkInterface::tGetPearsonCorrelation(int windowSize)
{	
	int n_rois = tGetNrOfROIs();
	if(!(n_rois > 1))
	{
		outputVector.clear();
		outputVector.append(0);
		return outputVector.toList();
	}
	TBV_Server_Request doRequest(tcpSocket,querryQueue,tr("tGetPearsonCorrelation"));
	doRequest.sendData("tGetPearsonCorrelation",windowSize);
	QString status = doRequest.getReply();

	if(!status.isEmpty())
	{
		outputVector.clear();
		outputVector.append(0);
		return outputVector.toList();
	}	
	int ncon=(n_rois*(n_rois-1))/2; // total number of correlations (edges)

	outputVector.reserve(ncon);
	outputVector.resize(ncon);

	doRequest.getVectorDataOfByteArray(&outputVector, ncon);
	outputVector.append(1);
	return outputVector.toList();
}

QList<double> TBVNetworkInterface::tGetPearsonCorrelationAtTimePoint(int windowSize, int timePoint)
{	
	int n_rois = tGetNrOfROIs();
	if(!(n_rois > 1))
	{
		outputVector.clear();
		outputVector.append(0);
		return outputVector.toList();
	}

	TBV_Server_Request doRequest(tcpSocket,querryQueue,tr("tGetPearsonCorrelationAtTimePoint"));
	doRequest.sendData("tGetPearsonCorrelationAtTimePoint",windowSize, timePoint);
	QString status = doRequest.getReply();

	if(!status.isEmpty())
	{
		outputVector.clear();
		outputVector.append(0);
		return outputVector.toList();
	}	
	int ncon=(n_rois*(n_rois-1))/2; // total number of correlations (edges)

	outputVector.reserve(ncon);
	outputVector.resize(ncon);

	doRequest.getVectorDataOfByteArray(&outputVector, ncon);
	outputVector.append(1);
	return outputVector.toList();
}

QList<double> TBVNetworkInterface::tGetPartialCorrelation(int windowSize)
{	
	int n_rois = tGetNrOfROIs();
	if(!(n_rois > 1))
	{
		outputVector.clear();
		outputVector.append(0);
		return outputVector.toList();
	}
	TBV_Server_Request doRequest(tcpSocket,querryQueue,tr("tGetPartialCorrelation"));
	doRequest.sendData("tGetPartialCorrelation",windowSize);
	QString status = doRequest.getReply();

	if(!status.isEmpty())
	{
		outputVector.clear();
		outputVector.append(0);
		return outputVector.toList();
	}	
	int ncon=(n_rois*(n_rois-1))/2; // total number of correlations (edges)

	outputVector.reserve(ncon);
	outputVector.resize(ncon);

	doRequest.getVectorDataOfByteArray(&outputVector, ncon);
	outputVector.append(1);
	return outputVector.toList();
}

QList<double> TBVNetworkInterface::tGetPartialCorrelationAtTimePoint(int windowSize, int timePoint)
{	
	int n_rois = tGetNrOfROIs();
	if(!(n_rois > 1))
	{
		outputVector.clear();
		outputVector.append(0);
		return outputVector.toList();
	}

	TBV_Server_Request doRequest(tcpSocket,querryQueue,tr("tGetPartialCorrelationAtTimePoint"));
	doRequest.sendData("tGetPartialCorrelationAtTimePoint",windowSize, timePoint);
	QString status = doRequest.getReply();

	if(!status.isEmpty())
	{
		outputVector.clear();
		outputVector.append(0);
		return outputVector.toList();
	}	
	int ncon=(n_rois*(n_rois-1))/2; // total number of correlations (edges)

	outputVector.reserve(ncon);
	outputVector.resize(ncon);

	doRequest.getVectorDataOfByteArray(&outputVector, ncon);
	outputVector.append(1);
	return outputVector.toList();
}



void TBVNetworkInterface::writeError(QAbstractSocket::SocketError Error)
 {
     switch (Error) {
	 case QAbstractSocket::RemoteHostClosedError:
		 emit connectionError(tr("The remote host closed the connection.\n Please restart the remote host."));
		 break;
     case QAbstractSocket::HostNotFoundError:
         emit connectionError(tr("The host was not found. Please check the host name and port settings."));
         break;
     case QAbstractSocket::ConnectionRefusedError:
         emit connectionError(tr("The connection was refused by the peer. Make sure the server is running, and check that the host name and port settings are correct."));
         break;
     default:
         emit connectionError(tr("The following error occurred: %1. \nCheck the firewall settings!").arg(tcpSocket->errorString()));
     }

 }
