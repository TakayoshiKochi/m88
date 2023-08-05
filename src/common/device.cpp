// ---------------------------------------------------------------------------
//  Virtual Bus Implementation
//  Copyright (c) cisc 1999.
// ---------------------------------------------------------------------------
//  $Id: device.cpp,v 1.21 2000/06/20 23:53:03 cisc Exp $

#include "common/device.h"

#include <algorithm>

// #define LOGNAME "membus"
#include "common/diag.h"

// ---------------------------------------------------------------------------
//  DeviceList
//  状態保存・復帰の対象となるデバイスのリストを管理する．
//

// ---------------------------------------------------------------------------
//  リストにデバイスを登録
//
bool DeviceList::Add(IDevice* t) {
  ID id = t->GetID();
  if (!id)
    return false;

  auto n = FindNode(id);
  if (n != node_.end()) {
    ++n->count;
  } else {
    node_.emplace_back(t);
  }
  return true;
}

// ---------------------------------------------------------------------------
//  リストからデバイスを削除
//
bool DeviceList::Del(const ID id) {
  auto n = FindNode(id);
  if (n == node_.end())
    return false;

  if (--n->count == 0) {
    node_.erase(n);
  }
  return true;
}

// ---------------------------------------------------------------------------
//  指定された識別子を持つデバイスをリスト中から探す
//
IDevice* DeviceList::Find(const ID id) {
  auto n = FindNode(id);
  return n != node_.end() ? n->entry : nullptr;
}

// ---------------------------------------------------------------------------
//  指定された識別子を持つデバイスノードを探す
//
std::vector<DeviceList::Node>::iterator DeviceList::FindNode(const ID id) {
  return std::find_if(node_.begin(), node_.end(), [id](Node n) { return n.entry->GetID() == id; });
}

// ---------------------------------------------------------------------------
//  状態保存に必要なデータサイズを求める
//
uint32_t DeviceList::GetStatusSize() {
  uint32_t size = sizeof(Header);
  for (auto n : node_) {
    int ds = n.entry->GetStatusSize();
    if (ds)
      size += sizeof(Header) + ((ds + 3) & ~3);
  }
  return size;
}

// ---------------------------------------------------------------------------
//  状態保存を行う
//  data にはあらかじめ GetStatusSize() で取得したサイズのバッファが必要
//
bool DeviceList::SaveStatus(uint8_t* data) {
  for (auto n : node_) {
    int s = n.entry->GetStatusSize();
    if (s) {
      ((Header*)data)->id = n.entry->GetID();
      ((Header*)data)->size = s;
      data += sizeof(Header);
      n.entry->SaveStatus(data);
      data += (s + 3) & ~3;
    }
  }
  ((Header*)data)->id = 0;
  ((Header*)data)->size = 0;
  return true;
}

// ---------------------------------------------------------------------------
//  SaveStatus で保存した状態から復帰する．
//
bool DeviceList::LoadStatus(const uint8_t* data) {
  if (!CheckStatus(data))
    return false;
  while (1) {
    const Header* hdr = (const Header*)data;
    data += sizeof(Header);
    if (!hdr->id)
      break;

    IDevice* dev = Find(hdr->id);
    if (dev) {
      if (!dev->LoadStatus(data))
        return false;
    }
    data += (hdr->size + 3) & ~3;
  }
  return true;
}

// ---------------------------------------------------------------------------
//  状態データが現在の構成で適応可能か調べる
//  具体的にはサイズチェックだけ．
//
bool DeviceList::CheckStatus(const uint8_t* data) {
  while (1) {
    const Header* hdr = (const Header*)data;
    data += sizeof(Header);
    if (!hdr->id)
      break;

    IDevice* dev = Find(hdr->id);
    if (dev && dev->GetStatusSize() != hdr->size)
      return false;
    data += (hdr->size + 3) & ~3;
  }
  return true;
}
