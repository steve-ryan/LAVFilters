/*
 *      Copyright (C) 2011 Hendrik Leppkes
 *      http://www.1f0.de
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 */

#pragma once

#include <deque>

#include "StreamInfo.h"

#define DSHOW_TIME_BASE 10000000        // DirectShow times are in 100ns units
#define NO_SUBTITLE_PID DWORD_MAX

struct ILAVFSettings;

// Data Packet for queue storage
class Packet
{
public:
  static const REFERENCE_TIME INVALID_TIME = _I64_MIN;

  DWORD StreamId;
  BOOL bDiscontinuity, bSyncPoint, bAppendable;
  REFERENCE_TIME rtStart, rtStop;
  AM_MEDIA_TYPE* pmt;

  Packet() { pmt = NULL; m_pbData = NULL; bDiscontinuity = bSyncPoint = bAppendable = FALSE; rtStart = rtStop = INVALID_TIME; m_dSize = 0; }
  ~Packet() { DeleteMediaType(pmt); free(m_pbData); }

  // Getter
  DWORD GetDataSize() const { return m_dSize; }
  BYTE *GetData() { return m_pbData; }
  BYTE GetAt(DWORD pos) const { return m_pbData[pos]; }
  bool IsEmpty() const { return m_dSize == 0; }

  // Setter
  void SetDataSize(DWORD len) { m_dSize = len; m_pbData = (BYTE *)realloc(m_pbData, len); }
  void SetData(const void* ptr, DWORD len) { SetDataSize(len); memcpy(m_pbData, ptr, len); }

  // Append the data of the package to our data buffer
  void Append(Packet *ptr) {
    DWORD prevSize = m_dSize;
    SetDataSize(prevSize + ptr->GetDataSize());
    memcpy(m_pbData+prevSize, ptr->GetData(), ptr->GetDataSize());
  }

  // Remove count bytes from position index
  void RemoveHead(DWORD count) {
    count = min(count, m_dSize);
    memmove(m_pbData, m_pbData+count, m_dSize-count);
    SetDataSize(m_dSize - count);
  }

private:
  DWORD m_dSize;
  BYTE *m_pbData;
};

class CBaseDemuxer : public CUnknown
{
public:
  enum StreamType {video, audio, subpic, unknown};

  DECLARE_IUNKNOWN

  // Demuxing Methods (pure virtual)

  // Open the file
  virtual STDMETHODIMP Open(LPCOLESTR pszFileName) = 0;
  // Get Duration
  virtual REFERENCE_TIME GetDuration() const = 0;
  // Get the next packet from the file
  virtual STDMETHODIMP GetNextPacket(Packet **ppPacket) = 0;
  // Seek to the given position
  virtual STDMETHODIMP Seek(REFERENCE_TIME rTime) = 0;
  // Get the container format
  virtual const char *GetContainerFormat() const = 0;
  // Create Stream Description
  virtual HRESULT StreamInfo(DWORD streamId, LCID *plcid, WCHAR **ppszName) const = 0;

  // Select the active title
  virtual STDMETHODIMP SetTitle(int idx) { return E_NOTIMPL; }
  // Get Title Info
  virtual STDMETHODIMP GetTitleInfo(int idx, REFERENCE_TIME *rtDuration, WCHAR **ppszName) { return E_NOTIMPL; }
  // Title count
  virtual STDMETHODIMP GetNumTitles() { return E_NOTIMPL; }
  
  // Set the currently active stream of one type
  // The demuxers can use this to filter packets before returning back to the caller on GetNextPacket
  // This functionality is optional however, so the caller should not rely on only receiving packets
  // for active streams.
  virtual HRESULT SetActiveStream(StreamType type, int pid) { m_dActiveStreams[type] = pid; return S_OK; }

  // Called when the settings of the splitter change
  virtual void SettingsChanged(ILAVFSettings *pSettings) {};

public:
  struct stream {
    CStreamInfo *streamInfo;
    DWORD pid;
    struct stream() { streamInfo = NULL; pid = 0; }
    operator DWORD() const { return pid; }
    bool operator == (const struct stream& s) const { return (DWORD)*this == (DWORD)s; }
  };

  class CStreamList : public std::deque<stream>
  {
  public:
    static const WCHAR* ToStringW(int type);
    static const CHAR* ToString(int type);
    const stream* FindStream(DWORD pid);
    void Clear();
  };


protected:
  CBaseDemuxer(LPCTSTR pName, CCritSec *pLock);
  void CreateNoSubtitleStream();

public:
  // Get the StreamList of the correponding type
  virtual CStreamList *GetStreams(StreamType type) { return &m_streams[type]; }

  
  // Select the best video stream
  virtual const stream* SelectVideoStream() = 0;
  
  // Select the best audio stream
  virtual const stream* SelectAudioStream(std::list<std::string> prefLanguages) = 0;
  
  // Subtitle modes
#define SUBMODE_NO_SUBS 0
#define SUBMODE_FORCED_SUBS 1
#define SUBMODE_ALWAYS_SUBS 2
  // Select the best subtitle stream
  virtual const stream* SelectSubtitleStream(std::list<std::string> prefLanguages, int subtitleMode, BOOL bOnlyMatching) = 0;

protected:
  CCritSec *m_pLock;
  CStreamList m_streams[unknown];
  int m_dActiveStreams[unknown];
};
