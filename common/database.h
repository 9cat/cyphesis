// This file may be redistributed and modified only under the terms of
// the GNU General Public License (See COPYING for details).
// Copyright (C) 2001 Alistair Riddoch

#ifndef DATABSE_H
#define DATABSE_H

#include "config.h"

#ifdef HAVE_LIBDB_CXX
#include <db3/db_cxx.h>

#include <Atlas/Message/DecoderBase.h>

class Decoder : public Atlas::Message::DecoderBase {
    virtual void ObjectArrived(const Atlas::Message::Object& obj) {
        cout << "GOT OBJECT" << endl << flush;
        m_check = true;
        m_obj = obj;
    }
    bool m_check;
    Atlas::Message::Object m_obj;
  public:
    Decoder() : m_check (false) { }
    bool check() const { return m_check; }
    const Atlas::Message::Object & get() { m_check = false; return m_obj; }
};

class DatabaseIterator;

class Database {
  protected:
    static Database * m_instance;

    Db account_db;
    Db world_db;
    Db mind_db;
    Db server_db;
    Decoder m_d;

    Database() : account_db(NULL, DB_CXX_NO_EXCEPTIONS),
                   world_db(NULL, DB_CXX_NO_EXCEPTIONS),
                    mind_db(NULL, DB_CXX_NO_EXCEPTIONS),
                  server_db(NULL, DB_CXX_NO_EXCEPTIONS) { }

    bool decodeObject(Dbt & data, Atlas::Message::Object &);
    bool putObject(Db &, const Atlas::Message::Object &, const char * key);
    bool getObject(Db &, const char * key, Atlas::Message::Object &);
  public:
    static Database * instance();

    bool initAccount(bool create = false);
    bool initWorld(bool create = false);
    bool initMind(bool create = false);
    bool initServer(bool create = false);
    void shutdownAccount();
    void shutdownWorld();
    void shutdownMind();
    void shutdownServer();

    Db & getWorldDb() { return world_db; }
    Db & getMindDb() { return mind_db; }

    friend DatabaseIterator;
};

class DatabaseIterator {
  protected:
    Dbc * m_cursor;
    Db & m_db;

  public:
    DatabaseIterator(Db & db ) : m_db(db) {
        db.cursor(NULL, &m_cursor, 0);
    }
    bool get(Atlas::Message::Object & o);
    bool del();
};

#else

#error Cannot build without db3 currently

class Database {
  protected:
    Database() { }

    static Database * m_instance;
  public:
    static Database * instance();
};

#endif // HAVE_LIBDB_CXX

#endif // DATABSE_H
