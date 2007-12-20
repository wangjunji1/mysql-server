/* Copyright (C) 2007 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/* DbUtil.cpp: implementation of the database utilities class.*/

#include "DbUtil.hpp"

/* Constructors */

DbUtil::DbUtil(const char * dbname)
{
  m_port = 0;
  m_connected = false;
  this->setDbName(dbname);
}

DbUtil::DbUtil(const char * dbname, const char* suffix)
{
  this->setDbName(dbname);
  m_connected = false;

  const char* env= getenv("MYSQL_HOME");
  if (env && strlen(env))
  {
    default_file.assfmt("%s/my.cnf", env);
  }

  if (suffix != NULL){
    default_group.assfmt("client%s", suffix);
  }
  else {
    default_group.assign("client.1.master");
  }

  ndbout << "default_file: " << default_file.c_str() << endl;
  ndbout << "default_group: " << default_group.c_str() << endl;
}

/* Destructor*/

DbUtil::~DbUtil()
{
  this->databaseLogout();
}

/* Database Login */

void 
DbUtil::databaseLogin(const char* system, const char* usr,
                           const char* password, unsigned int portIn,
                           const char* sockIn, bool transactional)
{
  if (!(mysql = mysql_init(NULL)))
  {
    myerror("DB Login-> mysql_init() failed");
    exit(DBU_FAILED);
  }
  this->setUser(usr);
  this->setHost(system);
  this->setPassword(password);
  this->setPort(portIn);
  this->setSocket(sockIn);

  if (!(mysql_real_connect(mysql, 
                           m_host.c_str(), 
                           m_user.c_str(), 
                           m_pass.c_str(), 
                           "test", 
                           m_port, 
                           m_socket.c_str(), 0)))
  {
    myerror("connection failed");
    mysql_close(mysql);
    exit(DBU_FAILED);
  }

  mysql->reconnect = DBU_TRUE;

  /* set AUTOCOMMIT */
  if(!transactional)
    mysql_autocommit(mysql, DBU_TRUE);
  else
    mysql_autocommit(mysql, DBU_FALSE);

  #ifdef DEBUG
    printf("\n\tConnected to MySQL server version: %s (%lu)\n\n", 
           mysql_get_server_info(mysql),
           (unsigned long) mysql_get_server_version(mysql));
  #endif
}

/* Database Connect */

int 
DbUtil::connect()
{
  if (!(mysql = mysql_init(NULL)))
  {
    myerror("DB connect-> mysql_init() failed");
    return DBU_FAILED;
  }

  /* Load connection parameters file and group */
  if (mysql_options(mysql, MYSQL_READ_DEFAULT_FILE, default_file.c_str()) ||
      mysql_options(mysql, MYSQL_READ_DEFAULT_GROUP, default_group.c_str()))
  {
    myerror("DB Connect -> mysql_options failed");
    return DBU_FAILED;
  }

  /*
    Connect, read settings from my.cnf
    NOTE! user and password can be stored there as well
   */

  if (mysql_real_connect(mysql, NULL, "root","", m_dbname.c_str(), 
                         0, NULL, 0) == NULL)
  {
    myerror("connection failed");
    mysql_close(mysql);
    return DBU_FAILED;
  }

  m_connected = true;
  return DBU_OK;
}


/* Database Logout */

void 
DbUtil::databaseLogout()
{
  if (mysql){
    #ifdef DEBUG
      printf("\n\tClosing the MySQL database connection ...\n\n");
    #endif
    mysql_close(mysql);
  }
}

/* Prepare MySQL Statements Cont */

MYSQL_STMT *STDCALL 
DbUtil::mysqlSimplePrepare(const char *query)
{
  #ifdef DEBUG
    printf("Inside DbUtil::mysqlSimplePrepare\n");
  #endif
  int m_res = DBU_OK;

  MYSQL_STMT *my_stmt= mysql_stmt_init(this->getMysql());
  if (my_stmt && (m_res = mysql_stmt_prepare(my_stmt, query, strlen(query)))){
    this->printStError(my_stmt,"Prepare Statement Failed");
    mysql_stmt_close(my_stmt);
    exit(DBU_FAILED);
  }
  return my_stmt;
}

/* Close MySQL Statements Handle */

void 
DbUtil::mysqlCloseStmHandle(MYSQL_STMT *my_stmt)
{
  mysql_stmt_close(my_stmt);
}
 
/* Error Printing */

void 
DbUtil::printError(const char *msg)
{
  if (this->getMysql() && mysql_errno(this->getMysql()))
  {
    if (this->getMysql()->server_version)
      printf("\n [MySQL-%s]", this->getMysql()->server_version);
    else
      printf("\n [MySQL]");
      printf("[%d] %s\n", this->getErrorNumber(), this->getError());
  }
  else if (msg)
    printf(" [MySQL] %s\n", msg);
}

void 
DbUtil::printStError(MYSQL_STMT *stmt, const char *msg)
{
  if (stmt && mysql_stmt_errno(stmt))
  {
    if (this->getMysql() && this->getMysql()->server_version)
      printf("\n [MySQL-%s]", this->getMysql()->server_version);
    else
      printf("\n [MySQL]");

    printf("[%d] %s\n", mysql_stmt_errno(stmt),
    mysql_stmt_error(stmt));
  }
  else if (msg)
    printf("[MySQL] %s\n", msg);
}

/* Select which database to use */

int 
DbUtil::select_DB()
{
  return mysql_select_db(this->getMysql(), this->getDbName());
}

/* Run Simple Queries */

int 
DbUtil::doQuery(char * stm)
{
  return mysql_query(this->getMysql(), stm);
}

int 
DbUtil::doQuery(const char * stm)
{
  return mysql_query(this->getMysql(), stm);
}

/* Return MySQL Error String */

const char * 
DbUtil::getError()
{
  return mysql_error(this->getMysql());
}

/* Retrun MySQL Error Number */

int 
DbUtil::getErrorNumber()
{
  return mysql_errno(this->getMysql());
}

/* Count Table Rows */

unsigned long 
DbUtil::selectCountTable(const char * table)
{
  unsigned long m_count = 0;
  BaseString m_query;
       
  m_query.assfmt("select count(*) from %s", table);
  if (mysql_query(this->getMysql(),m_query.c_str()) || 
      !(m_result=mysql_store_result(this->getMysql())))
  {
    this->printError("selectCountTable\n");
    return DBU_FAILED;
  }
  m_row = mysql_fetch_row(m_result);
  m_count = (ulong) strtoull(m_row[0], (char**) 0, 10);
  mysql_free_result(m_result);
    
  return m_count;
}

/* DIE */

void 
DbUtil::die(const char *file, int line, const char *expr)
{
  printf("%s:%d: check failed: '%s'\n", file, line, expr);
  abort();
}

/* EOF */

