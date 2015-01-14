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

#include <QDataStream>
#include <QtNetwork>
#include "tbv_server_request.h"

class TBVNetworkInterface : public QThread
{
	Q_OBJECT

public:
	TBVNetworkInterface(bool autoConnect = false, bool autoReconnect = true);
	~TBVNetworkInterface();

	//Basic Project Functions
	int tGetCurrentTimePoint();
	int tGetExpectedNrOfTimePoints();
	QList<int> tGetDimsOfFunctionalData();
	QString tGetProjectName();
	QString tGetWatchFolder();
	QString tGetTargetFolder();
	QString tGetFeedbackFolder();

	//Protocol, DM, GLM Functions:
	int tGetCurrentProtocolCondition();
	int tGetFullNrOfPredictors();
	int tGetCurrentNrOfPredictors();
	int tGetNrOfConfoundPredictors();
	float tGetValueOfDesignMatrix(int pred, int timepoint);
	int tGetNrOfContrasts();

	//ROI Functions
	int tGetNrOfROIs();
	float tGetMeanOfROI(int roi);
	QList<float> tGetExistingMeansOfROI(int roi, int toTimePoint); //toCheck
	float tGetMeanOfROIAtTimePoint(int roi, int timepoint);
	int tGetNrOfVoxelsOfROI(int roi);
	float tGetBetaOfROI(int roi,int beta);
	QList<int> tGetCoordsOfVoxelOfROI(int roi, int voxel);
	QList<int> tGetAllCoordsOfVoxelsOfROI(int roi);

	//Volume Data Access Functions
	float tGetValueOfVoxelAtTime(int x, int y, int z, int timepoint);
	QList<short> tGetValueOfAllVoxelsAtTime(int timepoint);
	QList<short> tGetRawValueOfAllVoxelsAtTime(int timepoint);
	double tGetBetaOfVoxel(int beta, int x, int y, int z);
	QList<double> tGetBetaMaps();
	float tGetMapValueOfVoxel(int map, int x, int y, int z);
	QList<float> tGetContrastMaps();

	//SVM Access Functions
	int tGetNumberOfClasses();
	QList<float> tGetCurrentClassifierOutput();

	//Functional Connectivity
	QList<double> tGetPearsonCorrelation(int windowSize);
	QList<double> tGetPearsonCorrelationAtTimePoint(int windowSize, int timePoint);
	QList<double> tGetPartialCorrelation(int windowSize);
	QList<double> tGetPartialCorrelationAtTimePoint(int windowSize, int timePoint);

	bool connectToServer(char *ipAddress,quint16 port);
	bool disconnectFromServer();

	bool isConnected();

	bool setAutoConnection(bool setting);

private:
	quint16 nDefaultPort;
	QUdpSocket *udpSocket;
	QTcpSocket *tcpSocket;
	QTcpSocket *rTcpSocket;
	qint64 blockSize;
	QQueue<QString> *querryQueue;
	QVector<short int> TimeCourseData;
	QVector<short int> RawTimeCourseData;
	QVector<float> AllMeansOfROI;
	QVector<double> outputVector;
	QVector<double> BetaMaps;
	QVector<float> ContrastMaps;
	QVector<int> CoordsOfVoxelsOfROI;
	QVector<float> ClassifierOutput;
	bool sendStreamDefinition(char *definition,QTcpSocket *dSocket);
	bool checkConnection(QString socketName, QTcpSocket *dSocket);
	bool udpAutoConnect, reConnect, terminateReconnect, terminateReciever, manualDisconnect;
	QString ipAddressString;
	quint16 iPort;
	void tryToReconnect();
	int VMajor,VMinor,VSubminor;

signals:
	void executePreStep(int timepoint);
	void executePostStep(int timepoint);
	void executePostRun(int timepoint);
	void disconnected();
	void connected(QString iPAddress, quint16 port);
	void connectionError(QString error);
	void connecting();

private slots:
	void readExecuteStep();
	void connectionEstablished();
	void connectionLost();
	void writeError(QAbstractSocket::SocketError Error);
	void processPendingDatagrams();
};