/**
*    Copyright (C) 2008 10gen Inc.
*
*    This program is free software: you can redistribute it and/or  modify
*    it under the terms of the GNU Affero General Public License, version 3,
*    as published by the Free Software Foundation.
*
*    This program is distributed in the hope that it will be useful,
*    but WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*    GNU Affero General Public License for more details.
*
*    You should have received a copy of the GNU Affero General Public License
*    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "stdafx.h"
#include "../cmdline.h"
#include "health.h"
#include "../commands.h"
#include "replset.h"
#include "rs_config.h"

namespace mongo { 

    class CmdReplSetInitiate : public Command { 
    public:
        virtual LockType locktype() const { return WRITE; }
        virtual bool slaveOk() const { return true; }
        virtual bool adminOnly() const { return true; }
        virtual bool logTheOp() { return false; }
        CmdReplSetInitiate() : Command("replSetInitiate") { }
        virtual void help(stringstream& h) const { 
            h << "Initiate/christen a replica set."; 
            h << "\nhttp://www.mongodb.org/display/DOCS/Replica+Set+Commands";
        }
        virtual bool run(const char *ns, BSONObj& cmdObj, string& errmsg, BSONObjBuilder& result, bool fromRepl) {
            if( !replSet ) { 
                errmsg = "server is not running with --replSet";
                return false;
            }
            if( theReplSet ) {
                errmsg = "already initialized";
                return false;
            }            
            if( ReplSet::startupStatus == ReplSet::BADCONFIG ) {
                errmsg = "server already in BADCONFIG state (check logs); not initiating";
                result.append("info", ReplSet::startupStatusMsg);
                return false;
            }
            if( ReplSet::startupStatus != ReplSet::EMPTYCONFIG ) {
                result.append("startupStatus", ReplSet::startupStatus);
                errmsg = "all seed hosts must be reachable to initiate set";
                return false;
            }

            ReplSetConfig newConfig(cmdObj["replSetInitiate"].Obj());

            return true;
        }
    } cmdReplSetInitiate;

    /* commands in other files:
         replSetHeartbeat - health.cpp
    */

    class CmdReplSetGetStatus : public Command {
    public:
        virtual bool slaveOk() const { return true; }
        virtual bool adminOnly() const { return true; }
        virtual bool logTheOp() { return false; }
        virtual LockType locktype() const { return NONE; }
        virtual void help( stringstream &help ) const {
            help << "Report status of a replica set from the POV of this server\n";
            help << "{ replSetGetStatus : 1 }";
            help << "\nhttp://www.mongodb.org/display/DOCS/Replica+Set+Commands";
        }

        CmdReplSetGetStatus() : Command("replSetGetStatus", true) { }
        virtual bool run(const char *ns, BSONObj& cmdObj, string& errmsg, BSONObjBuilder& result, bool fromRepl) {
            if( !replSet ) { 
                errmsg = "not running with --replSet";
                return false;
            }
            if( theReplSet == 0 ) {
                result.append("startupStatus", ReplSet::startupStatus);
                errmsg = ReplSet::startupStatusMsg.empty() ? 
                    errmsg = "replset unknown error 1" : ReplSet::startupStatusMsg;
                return false;
            }

            theReplSet->summarizeStatus(result);

            return true;
        }
    } cmdReplSetGetStatus;

    class CmdReplSetFreeze : public Command {
    public:
        virtual bool slaveOk() const { return true; }
        virtual bool adminOnly() const { return true; }
        virtual bool logTheOp() { return false; }
        virtual LockType locktype() const { return NONE; }
        virtual void help( stringstream &help ) const {
            help << "Enable / disable failover for the set - locks current primary as primary even if issues occur.\nFor use during system maintenance.\n";
            help << "{ replSetFreeze : <bool> }";
            help << "\nhttp://www.mongodb.org/display/DOCS/Replica+Set+Commands";
        }

        CmdReplSetFreeze() : Command("replSetFreeze", true) { }
        virtual bool run(const char *ns, BSONObj& cmdObj, string& errmsg, BSONObjBuilder& result, bool fromRepl) {
            if( !replSet ) { 
                errmsg = "not running with --replSet";
                return false;
            }
            if( theReplSet == 0 ) {
                result.append("startupStatus", ReplSet::startupStatus);
                errmsg = ReplSet::startupStatusMsg.empty() ? 
                    errmsg = "replset unknown error 1" : ReplSet::startupStatusMsg;
                return false;
            }

            errmsg = "not yet implemented"; /*TODO*/
            return false;
        }
    } cmdReplSetFreeze;

}
