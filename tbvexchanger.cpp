//Copyright (C) 2015  Michael Luehrs, Brain Innovation B.V. and Sven Gijsen
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

#include "tbvexchanger.h"

QScriptValue TBVExchanger::ctor_TBVExchanger(QScriptContext* context, QScriptEngine* engine)
{
	Q_UNUSED(context);
	//this function gets called first whenever a new object is constructed trough the script

	//	if (context->isCalledAsConstructor()) {
	//		// initialize the new object
	//		//context->thisObject().setProperty("bar", ...);
	//		// ...
	//		// return a non-object value to indicate that the
	//		// thisObject() should be the result of the "new Foo()" expression
	//		//return engine->undefinedValue();

	return engine->newQObject(new TBVExchanger(), QScriptEngine::ScriptOwnership);//Now call the below real Object constructor
} 

TBVExchanger::TBVExchanger(bool autoConnect, bool autoReconnect, QObject *parent) : QObject(parent)
{
	currentScriptEngine = NULL;
	tbvNetwIntFace = NULL;
	initialize(autoConnect,autoReconnect);
}

TBVExchanger::~TBVExchanger()
{
	//You don't have to manually disconnect signals and slots, the QObject destruction cleans them up automatically
	//disconnect(tbvNetwIntFace,&TBVNetworkInterface::executePreStep,this,&TBVExchanger::executePreStep);
	//disconnect(tbvNetwIntFace,&TBVNetworkInterface::executePostStep,this,&TBVExchanger::executePostStep);
	//disconnect(tbvNetwIntFace,&TBVNetworkInterface::executePostRun,this,&TBVExchanger::executePostRun);
	//disconnect(tbvNetwIntFace,&TBVNetworkInterface::disconnected,this,&TBVExchanger::disconnected);
	//disconnect(tbvNetwIntFace,&TBVNetworkInterface::connected,this,&TBVExchanger::connected);
	//disconnect(tbvNetwIntFace,&TBVNetworkInterface::connectionError,this,&TBVExchanger::connectionError);
	if(tbvNetwIntFace)
	{
		delete tbvNetwIntFace;
		tbvNetwIntFace = NULL;
	}
}

bool TBVExchanger::makeThisAvailableInScript(QString strObjectScriptName, QObject *engine)
{
	if (engine)
	{
		currentScriptEngine = reinterpret_cast<QScriptEngine *>(engine);
		//QObject *someObject = this;//new MyObject;
		QScriptValue objectValue = currentScriptEngine->newQObject(this);
		currentScriptEngine->globalObject().setProperty(strObjectScriptName, objectValue);
		return true;
	}
	return false;
}

bool TBVExchanger::initialize(bool autoConnect, bool autoReconnect)
{
	//if(tbvNetwIntFace == NULL)
	//{
		tbvNetwIntFace = new TBVNetworkInterface(autoConnect,autoReconnect);
		//bool bResult = connect(tbvNetwIntFace,SIGNAL(executePreStep(int)),this,SLOT(executePreStep()));
		connect(tbvNetwIntFace,&TBVNetworkInterface::executePreStep,this,&TBVExchanger::executePreStep);
		connect(tbvNetwIntFace,&TBVNetworkInterface::executePostStep,this,&TBVExchanger::executePostStep);
		connect(tbvNetwIntFace,&TBVNetworkInterface::executePostRun,this,&TBVExchanger::executePostRun);
		connect(tbvNetwIntFace,&TBVNetworkInterface::disconnected,this,&TBVExchanger::disconnected);
		connect(tbvNetwIntFace,&TBVNetworkInterface::connected,this,&TBVExchanger::connected);
		connect(tbvNetwIntFace,&TBVNetworkInterface::connectionError,this,&TBVExchanger::connectionError);
	//}
	return true;
}

bool TBVExchanger::activateAutoConnection()
{
	return tbvNetwIntFace->setAutoConnection(true);
}

bool TBVExchanger::deactivateAutoConnection()
{
	return tbvNetwIntFace->setAutoConnection(false);
}

bool TBVExchanger::activateAutoReconnect()
{
	return tbvNetwIntFace->setAutoReconnect(true);
}

bool TBVExchanger::deactivateAutoReconnect()
{
	return tbvNetwIntFace->setAutoReconnect(false);
}

int TBVExchanger::tGetCurrentTimePoint() 
{ 
	return tbvNetwIntFace->tGetCurrentTimePoint(); 
}

int TBVExchanger::tGetExpectedNrOfTimePoints()
{ 
	return tbvNetwIntFace->tGetExpectedNrOfTimePoints(); 
}

QList<int> TBVExchanger::tGetDimsOfFunctionalData()
{ 
	return tbvNetwIntFace->tGetDimsOfFunctionalData();
}

QString TBVExchanger::tGetProjectName()
{ 
	return tbvNetwIntFace->tGetProjectName();
}

QString TBVExchanger::tGetWatchFolder() 
{ 
	return tbvNetwIntFace->tGetWatchFolder(); 
}

QString TBVExchanger::tGetTargetFolder() 
{ 
	return tbvNetwIntFace->tGetTargetFolder(); 
}

QString TBVExchanger::tGetFeedbackFolder() 
{ 
	return tbvNetwIntFace->tGetFeedbackFolder(); 
}

int TBVExchanger::tGetCurrentProtocolCondition() 
{ 
	return tbvNetwIntFace->tGetCurrentProtocolCondition(); 
}

int TBVExchanger::tGetFullNrOfPredictors() 
{ 
	return tbvNetwIntFace->tGetFullNrOfPredictors(); 
}

int TBVExchanger::tGetCurrentNrOfPredictors() 
{ 
	return tbvNetwIntFace->tGetCurrentNrOfPredictors(); 
}

int TBVExchanger::tGetNrOfConfoundPredictors() 
{ 
	return tbvNetwIntFace->tGetNrOfConfoundPredictors(); 
}

float TBVExchanger::tGetValueOfDesignMatrix(int pred, int timepoint) 
{ 
	return tbvNetwIntFace->tGetValueOfDesignMatrix(pred, timepoint); 
}

int TBVExchanger::tGetNrOfContrasts() 
{ 
	return tbvNetwIntFace->tGetNrOfContrasts(); 
}

int TBVExchanger::tGetNrOfROIs() 
{ 
	return tbvNetwIntFace->tGetNrOfROIs(); 
}

float TBVExchanger::tGetMeanOfROI(int roi) 
{ 
	return tbvNetwIntFace->tGetMeanOfROI(roi); 
}

QList<float> TBVExchanger::tGetExistingMeansOfROI(int roi, int toTimePoint)
{ 
	return tbvNetwIntFace->tGetExistingMeansOfROI(roi, toTimePoint); 
}

float TBVExchanger::tGetMeanOfROIAtTimePoint(int roi, int timePoint) 
{ 
	return tbvNetwIntFace->tGetMeanOfROIAtTimePoint(roi, timePoint); 
}

int TBVExchanger::tGetNrOfVoxelsOfROI(int roi) 
{ 
	return tbvNetwIntFace->tGetNrOfVoxelsOfROI(roi); 
}

float TBVExchanger::tGetBetaOfROI(int roi, int beta) 
{ 
	return tbvNetwIntFace->tGetBetaOfROI(roi, beta); 
}

QList<int> TBVExchanger::tGetCoordsOfVoxelOfROI(int roi, int voxel) 
{ 
	return tbvNetwIntFace->tGetCoordsOfVoxelOfROI(roi, voxel); 
}

QList<int> TBVExchanger::tGetAllCoordsOfVoxelsOfROI(int roi)
{
	return tbvNetwIntFace->tGetAllCoordsOfVoxelsOfROI(roi); 
}

float TBVExchanger::tGetValueOfVoxelAtTime(int x, int y, int z, int timepoint)
{
	return tbvNetwIntFace->tGetValueOfVoxelAtTime(x, y, z, timepoint);
}

QList<short> TBVExchanger::tGetValueOfAllVoxelsAtTime(int timepoint)
{
	return tbvNetwIntFace->tGetValueOfAllVoxelsAtTime(timepoint);
}

QList<short> TBVExchanger::tGetRawValueOfAllVoxelsAtTime(int timepoint)
{
	return tbvNetwIntFace->tGetRawValueOfAllVoxelsAtTime(timepoint);
}

double TBVExchanger::tGetBetaOfVoxel(int beta, int x, int y, int z)
{
	return tbvNetwIntFace->tGetBetaOfVoxel(beta, x, y, z);
}

QList<double> TBVExchanger::tGetBetaMaps()
{
	return tbvNetwIntFace->tGetBetaMaps();
}

float TBVExchanger::tGetMapValueOfVoxel(int map, int x, int y, int z)
{
	return tbvNetwIntFace->tGetMapValueOfVoxel(map, x, y, z);
}

QList<float> TBVExchanger::tGetContrastMaps()
{
	return tbvNetwIntFace->tGetContrastMaps();
}

int TBVExchanger::tGetNumberOfClasses()
{
	return tbvNetwIntFace->tGetNumberOfClasses();
}

QList<float> TBVExchanger::tGetCurrentClassifierOutput()
{
	return tbvNetwIntFace->tGetCurrentClassifierOutput();
}

QList<double> TBVExchanger::tGetPearsonCorrelation(int windowSize)
{
	return tbvNetwIntFace->tGetPearsonCorrelation(windowSize);
}

QList<double> TBVExchanger::tGetPearsonCorrelationAtTimePoint(int windowSize, int timePoint)
{
	return tbvNetwIntFace->tGetPearsonCorrelationAtTimePoint(windowSize, timePoint);
}

QList<double> TBVExchanger::tGetPartialCorrelation(int windowSize)
{
	return tbvNetwIntFace->tGetPartialCorrelation(windowSize);
}

QList<double> TBVExchanger::tGetPartialCorrelationAtTimePoint(int windowSize, int timePoint)
{
	return tbvNetwIntFace->tGetPartialCorrelationAtTimePoint(windowSize, timePoint);
}

bool TBVExchanger::connectToServer(QString sIPAddress, int port)
{
	return tbvNetwIntFace->connectToServer(sIPAddress.toLatin1().data(), (quint16)port);
}

bool TBVExchanger::disconnectFromServer()
{
	return tbvNetwIntFace->disconnectFromServer();
}
