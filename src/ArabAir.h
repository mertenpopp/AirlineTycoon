#pragma once
//============================================================================================
// ArabAir.h : Der Schalter und das Hinterzimmer von ArabAir!
//============================================================================================

#include "defines.h"
#include "SmackPrs.h"
#include "StdRaum.h"

class CArabAir : public CStdRaum {
    // Construction
  public:
    CArabAir(BOOL bHandy, ULONG PlayerNum);

    // Data
  public:
    CSmackerPerson SP_Araber;
    CAnimation FunkelAnim;
    SBBM GloveBm;

    SBFX StartupFX;
    SBFX RadioFX;

    SB_CFont FontBankBlack;
    SB_CFont FontBankRed;

    // Overrides
    // ClassWizard generated virtual function overrides
    //{{AFX_VIRTUAL(CArabAir)
    //}}AFX_VIRTUAL

    // Implementation
  public:
    virtual ~CArabAir();

    // Generated message map functions
  protected:
    //{{AFX_MSG(CArabAir)
    virtual void OnLButtonDown(UINT nFlags, CPoint point);
    virtual void OnPaint();
    virtual void OnRButtonDown(UINT nFlags, CPoint point);
    //}}AFX_MSG
    // DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
