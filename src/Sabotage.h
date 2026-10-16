#pragma once
//============================================================================================
// Sabotage.h : Sabotageraum
//============================================================================================

#include "defines.h"
#include "SmackPrs.h"
#include "StdRaum.h"

class CSabotage : public CStdRaum {
    // Construction
  public:
    CSabotage(BOOL bHandy, ULONG PlayerNum);

    // Operations
  public:
    /* The sounds come before the animations that play them: members are destroyed in reverse
       order, and ~CAnimation stops its sound. Declared the other way round, DampfAnim and
       DynamitAnim stopped ZischFx and LunteFx after those were already gone - and since the
       compiler drops the nulling of a member in its own destructor, that called into a released
       sound and crashed on leaving the room, now and then. */
    SBFX ZischFx;
    SBFX LunteFx;
    SBFX BackFx;
    CAnimation DampfAnim;
    CAnimation DynamitAnim;
    CAnimation KamelAnim;
    CAnimation LampeAnim;
    SLONG CurrentTip{}; // Dieser Tip wird gerade angezeigt

    SLONG PlayEyeAnim;
    SBBM AraberBm;
    SBBM DartBm;
    SBBM ZangeBm;

    CSmackerPerson SP_Araber;

    // Overrides
    // ClassWizard generated virtual function overrides
    //{{AFX_VIRTUAL(CSabotage)
    //}}AFX_VIRTUAL

    // Implementation
  public:
    virtual ~CSabotage();

    // Generated message map functions
  protected:
    //{{AFX_MSG(CSabotage)
    virtual void OnLButtonDown(UINT nFlags, CPoint point);
    virtual void OnPaint();
    virtual void OnRButtonDown(UINT nFlags, CPoint point);
    //}}AFX_MSG
    // DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
