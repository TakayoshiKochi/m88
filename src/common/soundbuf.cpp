// ---------------------------------------------------------------------------
//  class SoundBuffer
//  Copyright (C) cisc 1999.
// ---------------------------------------------------------------------------
//  $Id: soundbuf.cpp,v 1.5 1999/11/26 10:13:25 cisc Exp $

#include "common/soundbuf.h"

#include <algorithm>

// ---------------------------------------------------------------------------
//  Sound Buffer
//
SoundBuffer::SoundBuffer() : buffer_(0), buffer_size_(0), ch_(0) {
  fill_when_empty_ = true;
}

SoundBuffer::~SoundBuffer() {
  Cleanup();
}

bool SoundBuffer::Init(int nch, int bufsize) {
  CriticalSection::Lock lock(cs_);

  delete[] buffer_;
  buffer_ = nullptr;

  buffer_size_ = bufsize;
  ch_ = nch;
  read_ = 0;
  write_ = 0;

  if (ch_ && buffer_size_ > 0) {
    buffer_ = new Sample[ch_ * buffer_size_];
    if (!buffer_)
      return false;

    memset(buffer_, 0, ch_ * buffer_size_ * sizeof(Sample));
  }
  return true;
}

void SoundBuffer::Cleanup() {
  CriticalSection::Lock lock(cs_);

  delete[] buffer_;
  buffer_ = 0;
}

// ---------------------------------------------------------------------------
//  バッファに音を追加
//
void SoundBuffer::Put(int samples) {
  CriticalSection::Lock lock(cs_);
  if (buffer_)
    PutMain(samples);
}

void SoundBuffer::PutMain(int samples) {
  // リングバッファの空きを計算
  int free;
  if (read_ <= write_)
    free = buffer_size_ + read_ - write_;
  else
    free = read_ - write_;

  if (!fill_when_empty_ && (samples > free - 1)) {
    int skip = std::min(samples - free + 1, buffer_size_ - free);
    free += skip;
    read_ += skip;
    if (read_ > buffer_size_)
      read_ -= buffer_size_;
  }

  // 書きこむべきデータ量を計算
  samples = std::min(samples, free - 1);
  if (samples > 0) {
    // 書きこむ
    if (buffer_size_ - write_ >= samples) {
      // 一度で書ける場合
      Mix(buffer_ + write_ * ch_, samples);
    } else {
      // ２度に分けて書く場合
      Mix(buffer_ + write_ * ch_, buffer_size_ - write_, buffer_, samples - (buffer_size_ - write_));
    }
    write_ += samples;
    if (write_ >= buffer_size_)
      write_ -= buffer_size_;
  }
}

// ---------------------------------------------------------------------------
//  バッファから音を貰う
//
void SoundBuffer::Get(Sample* dest, int samples) {
  CriticalSection::Lock lock(cs_);
  if (buffer_) {
    while (samples > 0) {
      int xsize = std::min(samples, buffer_size_ - read_);

      int avail;
      if (write_ >= read_)
        avail = write_ - read_;
      else
        avail = buffer_size_ + write_ - read_;

      // 供給不足なら追加
      if (xsize <= avail || fill_when_empty_) {
        if (xsize > avail)
          PutMain(xsize - avail);
        memcpy(dest, buffer_ + read_ * ch_, xsize * ch_ * sizeof(Sample));
        dest += xsize * ch_;
        read_ += xsize;
      } else {
        if (avail > 0) {
          memcpy(dest, buffer_ + read_ * ch_, avail * ch_ * sizeof(Sample));
          dest += avail * ch_;
          read_ += avail;
        }
        memset(dest, 0, (xsize - avail) * ch_ * sizeof(Sample));
        dest += (xsize - avail) * ch_;
      }

      samples -= xsize;
      if (read_ >= buffer_size_)
        read_ -= buffer_size_;
    }
  }
}

// ---------------------------------------------------------------------------
//  バッファが空か，空に近い状態か?
//
bool SoundBuffer::IsEmpty() {
  int avail;
  if (write_ >= read_)
    avail = write_ - read_;
  else
    avail = buffer_size_ + write_ - read_;

  return avail == 0;
}
