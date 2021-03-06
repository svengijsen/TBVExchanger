//TBVExchanger
//Copyright (C) 2015  Sven Gijsen
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


#include "tbvexchangerplugin.h"
#include "tbvexchanger.h"
#include "defines.h"

Q_DECLARE_METATYPE(TBVExchanger*)
Q_DECLARE_METATYPE(TBVExchanger)

TBVExchangerPlugin::TBVExchangerPlugin(QObject *parent)
{
	TBVExchangerObject = new TBVExchanger(parent);
	TBVExchangerDiagObject = new TBVExchanger_Dialog();	
	strPluginInformation = QString(PLUGIN_INTERNAL_NAME) + " Plugin" + "(v" + PLUGIN_FILE_VERSION_STRING + ")";// +" by " + PLUGIN_AUTHOR_NAME;
	strPluginInternalName = PLUGIN_INTERNAL_NAME;
}

TBVExchangerPlugin::~TBVExchangerPlugin()
{
	delete TBVExchangerDiagObject;
	delete TBVExchangerObject;
}

int TBVExchangerPlugin::ConfigureScriptEngine(QScriptEngine &engine)
{
	QScriptValue TBVExchangerProto = engine.newQObject(TBVExchangerObject);
	engine.setDefaultPrototype(qMetaTypeId<TBVExchanger*>(), TBVExchangerProto);
	QScriptValue TBVExchangerCtor = engine.newFunction(TBVExchanger::ctor_TBVExchanger, TBVExchangerProto);
	engine.globalObject().setProperty(PLUGIN_SCRIPTOBJECT_NAME, TBVExchangerCtor);
	int nMetaType = qRegisterMetaType<TBVExchanger>(PLUGIN_SCRIPTOBJECT_CLASS);
	return nMetaType;
	//FinalizeScriptObjectScriptContext(engine, TBVExchangerObject);
}

bool TBVExchangerPlugin::ShowGUI()
{
	int returnVal;
	TBVExchangerDiagObject->setWindowTitle(strPluginInformation);
	returnVal = TBVExchangerDiagObject->exec();

	switch (returnVal) {
	case QMessageBox::Save:
	   // Save was clicked
	   break;
	case QMessageBox::Discard:
		// Don't Save was clicked
	    break;
	case QMessageBox::Cancel:
	    // Cancel was clicked
	    break;
	default:
	    // should never be reached
	    break;
	}		
	return true;
}

