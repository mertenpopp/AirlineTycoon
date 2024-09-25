#pragma once
// Outro.cpp : Der Render-Outro

/////////////////////////////////////////////////////////////////////////////
// COutro window

#include "CVideo.h"

class COutro : public CVideo {
    // Construction
  public:
    COutro(const CString &SmackName) : CVideo(SmackName) {}

    void OnVideoDone() override;
    void OnVideoCancel() override;
};

/////////////////////////////////////////////////////////////////////////////
