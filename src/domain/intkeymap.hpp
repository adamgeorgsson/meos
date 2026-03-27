#pragma once
// intkeymap.hpp — Custom open-addressing hash map used by oDataContainer.
// Copied from legacy code/intkeymap.hpp; removed StdAfx.h dependency.

template<class T, class KEY = int> class intkeymap {
private:
  const static KEY NoKey = -1013;

  struct keypair {
    KEY key;
    T value;
  };
  T dummy;
  T tmp;
  keypair *keys;
  unsigned siz;
  unsigned used;
  intkeymap *next;
  intkeymap *parent;
  double allocFactor;
  T noValue;
  unsigned hash1;
  unsigned hash2;
  int level;
  static int optsize(int arg);

  T &rehash(int size, KEY key, const T &value);
  T &get(const KEY key);

  void *lookup(KEY key) const;
public:
  virtual ~intkeymap();
  intkeymap(int size);
  intkeymap();
  intkeymap(const intkeymap &co);
  const intkeymap &operator=(const intkeymap &co);

  bool empty() const;
  int size() const;
  int getAlloc() const {return siz;}
  void clear();

  void resize(int size);
  int count(KEY key) {
    return lookup(key) ? 1 : 0;
  }
  bool lookup(KEY key, T &value) const;

  void insert(KEY key, const T &value);
  void remove(KEY key);
  void erase(KEY key) {remove(key);}
  const T operator[](KEY key) const
    {if (lookup(key, const_cast<T&>(tmp))) return tmp; else return T();}

  T &operator[](KEY key) {
    return get(key);
  }
};

#include "intkeymapimpl.hpp"
