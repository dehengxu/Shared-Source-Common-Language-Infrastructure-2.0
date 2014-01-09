// ==++==
//
//   
//    Copyright (c) 2006 Microsoft Corporation.  All rights reserved.
//   
//    The use and distribution terms for this software are contained in the file
//    named license.txt, which can be found in the root of this distribution.
//    By using this software in any fashion, you are agreeing to be bound by the
//    terms of this license.
//   
//    You must not remove this notice, or any other, from this software.
//   
//
// ==--==
//*****************************************************************************
// File: frameinfo.cpp
//
// Code to find control info about a stack frame.
//
//*****************************************************************************

#include "stdafx.h"

// Include so we can get information out of ComMethodFrame

// Get a frame pointer from a RegDisplay.
FramePointer GetSP(REGDISPLAY * pRDSrc)
{
    FramePointer fp = FramePointer::MakeFramePointer(
        (LPVOID) GetRegdisplaySP(pRDSrc)
        IA64_ARG(GetRegdisplayBSP(pRDSrc)));

    return fp;
}

// Get a frame pointer from a RegDisplay.
FramePointer GetFramePointer(REGDISPLAY * pRDSrc)
{
    FramePointer fp = FramePointer::MakeFramePointer(GetRegdisplaySP(pRDSrc));

    return fp;
}


/* ------------------------------------------------------------------------- *
 * DebuggerFrameInfo routines
 * ------------------------------------------------------------------------- */

//struct DebuggerFrameData:  Contains info used by the DebuggerWalkStackProc
// to do a stack walk.  The info and pData fields are handed to the pCallback
// routine at each frame,
struct DebuggerFrameData
{
    // Initialize this struct. Only done at the start of a stackwalk.
    void Init(
        Thread * _pThread,
        FramePointer _targetFP,
        BOOL fIgnoreNonmethodFrames,
        DebuggerStackCallback _pCallback,
        void                    *_pData
    )
    {
        LEAF_CONTRACT;

        this->pCallback = _pCallback;
        this->pData = _pData;

        this->cRealCounter = 0;

        this->thread = _pThread;
        this->targetFP = _targetFP;
        this->targetFound = (_targetFP == LEAF_MOST_FRAME);
        this->info.context = _pThread->GetContext();

        this->ignoreNonmethodFrames = fIgnoreNonmethodFrames;

        this->fProvideInternalFrames = (fIgnoreNonmethodFrames != 0) && g_pDebugger->IsSISEnabled();

        this->fNeedToSendEnterManagedChain = false;
        this->fTrackingUMChain = false;
        this->fHitExitFrame = false;

        this->info.eStubFrameType = STUBFRAME_NONE;
        this->info.quickUnwind = false;

        this->info.frame     = NULL;
        this->needParentInfo = false;


#if defined(_DEBUG)
        this->previousFP = LEAF_MOST_FRAME;
#endif // _DEBUG
    }

    // True if we need the next CrawlFrame to fill out part of this FrameInfo's data.
    bool                    needParentInfo;

    // The FrameInfo that we'll dispatch to the pCallback. This matches against
    // the CrawlFrame for that frame that the callback belongs too.
    FrameInfo               info;

    // Regdisplay that the EE stackwalker is updating.
    REGDISPLAY              regDisplay;


#if defined(_DEBUG)
    // For debugging, track the previous FramePointer so we can assert that we're
    // making progress through the stack.
    FramePointer            previousFP;
#endif // _DEBUG

    bool                    fHitExitFrame;

private:
    bool                    fNeedToSendEnterManagedChain;

    // Flag set when we first stack-walk to decide if we want to ignore certain frames.
    // Stepping ignores these frames; provide user stacktraces doesn't.
    BOOL                    ignoreNonmethodFrames;

    // Do we want callbacks for internal frames?
    // Steppers generally don't. User stack-walk does.
    bool                    fProvideInternalFrames;

    bool                    fTrackingUMChain;
    REGDISPLAY              rdUMChainStart;
    FramePointer            fpUMChainEnd;

    // Thread that the stackwalk is for.
    Thread                  *thread;


    // Target FP indicates at what point in the stackwalk we'll start dispatching callbacks.
    // Naturally, if this is LEAF_MOST_FRAME, then all callbacks will be dispatched
    FramePointer            targetFP;
    bool                    targetFound;

    // Count # of callbacks we could have dispatched (assuming targetFP==LEAF_MOST_FRAME).
    // Useful for detecting leaf.
    int                     cRealCounter;

    // Callback & user-data supplied to that callback.
    DebuggerStackCallback   pCallback;
    void                    *pData;

    private:

    // Raw invoke. This just does some consistency asserts,
    // and invokes the callback if we're in the requested target range.
    StackWalkAction RawInvokeCallback(FrameInfo * pInfo)
    {
#ifdef _DEBUG
        _ASSERTE(pInfo != NULL);
        MethodDesc * md = pInfo->md;
        // Invoke the callback to the user. Log what we're invoking.
        LOG((LF_CORDB, LL_INFO10000, "DSWCallback: MD=%s,0x%p, Chain=%x, Stub=%x, Frame=0x%p, Internal=%d\n",
            ((md == NULL) ? "None" : md->m_pszDebugMethodName), md,
            pInfo->chainReason,
            pInfo->eStubFrameType,
            pInfo->frame, pInfo->internal));

        // Make sure we're providing a valid FrameInfo for the callback.
        pInfo->AssertValid();
#endif
        // Update counter. This provides a convenient check for leaf FrameInfo.
        this->cRealCounter++;


        // Only invoke if we're past the target.
        if (!this->targetFound && IsEqualOrCloserToLeaf(this->targetFP, this->info.fp))
        {
            this->targetFound = true;
        }

        if (this->targetFound)
        {
            return (pCallback)(pInfo, pData);
        }
        else
        {
            LOG((LF_CORDB, LL_INFO10000, "Not invoking yet.\n"));
        }

        return SWA_CONTINUE;
    }

public:
    // Invoke a callback. This may do extra logic to preserve the interface between
    // the LS stackwalker and the LS:
    // - don't invoke if we're not at the target yet
    // - send EnterManagedChains if we need it.
    StackWalkAction InvokeCallback(FrameInfo * pInfo)
    {
        // Track if we've sent any managed code yet.
        // If we haven't, then don't send the enter-managed chain. This catches cases
        // when we have leaf-most unmanaged chain.
        if ((pInfo->frame == NULL) && (pInfo->md != NULL))
        {
            this->fNeedToSendEnterManagedChain = true;
        }


        // Do tracking to decide if we need to send a Enter-Managed chain.
        if (pInfo->HasChainMarker())
        {
            if (pInfo->managed)
            {
                // If we're dispatching a managed-chain, then we don't need to send another one.
                fNeedToSendEnterManagedChain = false;
            }
            else
            {
                // If we're dispatching an UM chain, then send the Managed one.
                if (fNeedToSendEnterManagedChain)
                {
                    fNeedToSendEnterManagedChain = false;

                    FrameInfo f;

                    // Assume entry chain's FP is one pointer-width after the upcoming UM chain.
                    FramePointer fpRoot = FramePointer::MakeFramePointer(
                        (BYTE*) GetRegdisplaySP(&pInfo->registers) - sizeof(DWORD*)
                        IA64_ARG(LEAF_MOST_FRAME_BSP));

                    f.InitForEnterManagedChain(fpRoot);
                    if (RawInvokeCallback(&f) == SWA_ABORT)
                    {
                        return SWA_ABORT;
                    }
                }
            }
        }

        return RawInvokeCallback(pInfo);
    }

    // Note that we should start tracking an Unmanaged Chain.
    void BeginTrackingUMChain(FramePointer fpRoot, REGDISPLAY * pRDSrc)
    {
        LEAF_CONTRACT;

        _ASSERTE(!this->fTrackingUMChain);

        CopyREGDISPLAY(&this->rdUMChainStart, pRDSrc);

        this->fTrackingUMChain = true;
        this->fpUMChainEnd = fpRoot;
        this->fHitExitFrame = false;

        LOG((LF_CORDB, LL_EVERYTHING, "UM Chain starting at Frame=0x%p\n", this->fpUMChainEnd.GetSPValue()));

        // This UM chain may get cancelled later, so don't even worry about toggling the fNeedToSendEnterManagedChain bit here.
        // Invoke() will track whether to send an Enter-Managed chain or not.
    }

    // For various heuristics, we may not want to send an UM chain.
    void CancelUMChain()
    {
        LEAF_CONTRACT;

        _ASSERTE(this->fTrackingUMChain);
        this->fTrackingUMChain = false;
    }

    // True iff we're currently tracking an unmanaged chain.
    bool IsTrackingUMChain()
    {
        LEAF_CONTRACT;

        return this->fTrackingUMChain;
    }



    // Get/Set Regdisplay that starts an Unmanaged chain.
    REGDISPLAY * GetUMChainStartRD()
    {
        LEAF_CONTRACT;
        _ASSERTE(fTrackingUMChain);
        return &rdUMChainStart;
    }

    // Get/Set FramePointer that ends an unmanaged chain.
    void SetUMChainEnd(FramePointer fp)
    {
        LEAF_CONTRACT;
        _ASSERTE(fTrackingUMChain);
        fpUMChainEnd = fp;
    }

    FramePointer GetUMChainEnd()
    {
        LEAF_CONTRACT;
        _ASSERTE(fTrackingUMChain);
        return fpUMChainEnd;
    }

    // Get thread we're currently tracing.
    Thread * GetThread()
    {
        LEAF_CONTRACT;
        return thread;
    }

    // Returns true if we're on the leaf-callback (ie, we haven't dispatched a callback yet.
    bool IsLeafCallback()
    {
        LEAF_CONTRACT;
        return cRealCounter == 0;
    }

    bool ShouldProvideInternalFrames()
    {
        LEAF_CONTRACT;
        return fProvideInternalFrames;
    }
    bool ShouldIgnoreNonmethodFrames()
    {
        LEAF_CONTRACT;
        return ignoreNonmethodFrames != 0;
    }
};

    #define ADJUST_REL_OFFSET(x, _dummy)    (x)->GetRelOffset()



bool HasExitRuntime(Frame *pFrame, DebuggerFrameData *pData, FramePointer *pPotentialFP)
{
    CONTRACTL
    {
        NOTHROW;
        GC_NOTRIGGER; // Callers demand this function be GC_NOTRIGGER.
    }
    CONTRACTL_END;

    TADDR returnIP, returnSP;

    EX_TRY
    {
        // This is a real bug. This may be called while holding GC-forbid locks, and so 
        // this function can't trigger a GC. However, the only impl we have calls GC-trigger functions.
        CONTRACT_VIOLATION(GCViolation);
        pFrame->GetUnmanagedCallSite(NULL, &returnIP, &returnSP);
    }
    EX_CATCH
    {
        // We never expect an actual exception here (maybe in oom).
        // If we get an exception, then simulate the default behavior for GetUnmanagedCallSite.
        returnIP = NULL;
        returnSP = NULL; // this will cause us to return true.
    }
    EX_END_CATCH(SwallowAllExceptions);

    LOG((LF_CORDB, LL_INFO100000,
         "DWSP: TYPE_EXIT: returnIP=0x%08x, returnSP=0x%08x, frame=0x%08x, threadFrame=0x%08x, regSP=0x%08x\n",
         returnIP, returnSP, pFrame, pData->GetThread()->GetFrame(), GetRegdisplaySP(&pData->regDisplay)));

    if (pPotentialFP != NULL)
    {
        *pPotentialFP = FramePointer::MakeFramePointer((void*)returnSP);
    }

    return ((pFrame != pData->GetThread()->GetFrame()) ||
            (returnSP == NULL) ||
            ((TADDR)GetRegdisplaySP(&pData->regDisplay) <= returnSP));

}

#ifdef _DEBUG

//-----------------------------------------------------------------------------
// Debug helpers to get name of Frame.
//-----------------------------------------------------------------------------
LPCUTF8 FrameInfo::DbgGetClassName()
{
    return (md == NULL) ? ("None") : (md->m_pszDebugClassName);
}
LPCUTF8 FrameInfo::DbgGetMethodName()
{
    return (md == NULL) ? ("None") : (md->m_pszDebugMethodName);
}


//-----------------------------------------------------------------------------
// Debug helper to asserts invariants about a FrameInfo before we dispatch it.
//-----------------------------------------------------------------------------
void FrameInfo::AssertValid()
{
    LEAF_CONTRACT;

    bool fMethod    = this->HasMethodFrame();
    bool fStub      = this->HasStubFrame();
    bool fChain     = this->HasChainMarker();

    // Can't be both Stub & Chain
    _ASSERTE(!fStub || !fChain);

    // Must be at least a Method, Stub or Chain or Internal
    _ASSERTE(fMethod || fStub || fChain || this->internal);

    // Check Managed status is consistent
    if (fMethod)
    {
        _ASSERTE(this->managed); // We only report managed methods
    }
    if (fChain)
    {
        if (!managed)
        {
            _ASSERTE((this->chainReason == CHAIN_THREAD_START) ||
                     (this->chainReason == CHAIN_ENTER_UNMANAGED));
        }
        else
        {
            _ASSERTE((this->chainReason != CHAIN_ENTER_UNMANAGED));
        }

    }

    // FramePointer should be valid
    _ASSERTE(this->fp != LEAF_MOST_FRAME);
    _ASSERTE(this->fp != ROOT_MOST_FRAME || (chainReason== CHAIN_THREAD_START));

    // If we have a Method, then we need an AppDomain.
    // (RS will need it to do lookup)
    if (fMethod)
    {
        _ASSERTE(currentAppDomain != NULL);
        _ASSERTE(managed);
    }

    if (fStub)
    {
        // All stubs (except LightWeightFunctions) match up w/a Frame.
        _ASSERTE(this->frame || (eStubFrameType == STUBFRAME_LIGHTWEIGHT_FUNCTION));
    }
}
#endif

//-----------------------------------------------------------------------------
// Get the DJI associated w/ this frame. This is a convenience function.
// This is recommended over using MethodDescs because DJI's are version-aware.
//-----------------------------------------------------------------------------
DebuggerJitInfo * FrameInfo::GetJitInfoFromFrame()
{
    CONTRACTL
    {
        NOTHROW;
        GC_NOTRIGGER;
    }
    CONTRACTL_END;

    // Not all FrameInfo objects correspond to actual code.
    if (HasChainMarker() || HasStubFrame() || (frame != NULL))
    {
        return NULL;
    }

    DebuggerJitInfo *ji = NULL;

    EX_TRY
    {
        _ASSERTE(this->md != NULL);
        ji = g_pDebugger->GetJitInfo(this->md, (const BYTE*)GetControlPC(&(this->registers)));
        _ASSERTE(ji != NULL);
        _ASSERTE(ji->m_fd == this->md);
    }
    EX_CATCH
    {
        ji = NULL;
    }
    EX_END_CATCH(SwallowAllExceptions);
    
    return ji;
}

//-----------------------------------------------------------------------------
// Get the DMI associated w/ this frame. This is a convenience function.
// DMIs are 1:1 with the (token, module) pair. 
//-----------------------------------------------------------------------------
DebuggerMethodInfo * FrameInfo::GetMethodInfoFromFrameOrThrow()
{
    CONTRACTL
    {
        THROWS;
        GC_NOTRIGGER;
    }
    CONTRACTL_END;

    MethodDesc * pDesc = this->md;
    mdMethodDef token = pDesc-> GetMemberDef();
    Module * pRuntimeModule = pDesc->GetModule();

    DebuggerMethodInfo *dmi = g_pDebugger->GetOrCreateMethodInfo(pRuntimeModule, token);
    return dmi;
}


//-----------------------------------------------------------------------------
// Init a FrameInfo for a UM chain.
// We need a stackrange to give to an unmanaged debugger.
// pRDSrc->Esp will provide the start (leaf) marker.
// fpRoot will provide the end (root) portion.
//-----------------------------------------------------------------------------
void FrameInfo::InitForUMChain(FramePointer fpRoot, REGDISPLAY * pRDSrc)
{
    _ASSERTE(pRDSrc != NULL);

    // Mark that we're an UM Chain (and nothing else).
    this->frame = NULL;
    this->md = NULL;

    // Fp will be the end (root) of the stack range.
    // pRDSrc->Sp will be the start (leaf) of the stack range.
    CopyREGDISPLAY(&(this->registers), pRDSrc);
    this->fp = fpRoot;

    this->quickUnwind = false;
    this->internal = false;
    this->managed = false;

    // These parts of the FrameInfo can be ignored for a UM chain.
    this->context = NULL;
    this->relOffset = 0;
    this->pIJM = NULL;
    this->MethodToken = NULL;
    this->currentAppDomain = NULL;
    this->exactGenericArgsToken = NULL;


    this->chainReason    = CHAIN_ENTER_UNMANAGED;
    this->eStubFrameType = STUBFRAME_NONE;

#ifdef _DEBUG
    FramePointer fpLeaf = GetSP(pRDSrc);
    _ASSERTE(IsCloserToLeaf(fpLeaf, fpRoot));
#endif

#ifdef _DEBUG
    // After we just init it, it had better be valid.
    this->AssertValid();
#endif
}


//-----------------------------------------------------------------------------
// Init a FrameInfo for a stub
//-----------------------------------------------------------------------------
void FrameInfo::InitForScratchFrameInfo()
{
}

void FrameInfo::InitFromStubHelper(
    CrawlFrame * pCF,
    MethodDesc * pMDHint, // NULL ok
    CorDebugInternalFrameType type
)
{
    _ASSERTE(pCF != NULL);

    Frame * pFrame = pCF->GetFrame();

    LOG((LF_CORDB, LL_EVERYTHING, "InitFromStubHelper. Frame=0x%p, type=%d\n", pFrame, type));

    // All Stubs have a Frame except for LightWeight methods
    _ASSERTE((type == STUBFRAME_LIGHTWEIGHT_FUNCTION) || (pFrame != NULL));
    REGDISPLAY *pRDSrc = pCF->GetRegisterSet();

    this->frame = pFrame;

    // Stub frames may be associated w/ a Method (as a hint). However this method
    // will never have a JitManager b/c it will never have IL (if it had IL, we'd be a
    // regulare frame, not a stub frame)
    this->md = pMDHint;

    CopyREGDISPLAY(&this->registers, pRDSrc);

    // FramePointer must match up w/ an EE Frame b/c that's how we match
    // we Exception callbacks.
    if (pFrame != NULL)
    {
        this->fp = FramePointer::MakeFramePointer(
            (LPVOID) pFrame
            IA64_ARG(LEAF_MOST_FRAME_BSP));
    }
    else
    {
        this->fp = GetSP(pRDSrc);
    }

    this->quickUnwind = false;
    this->internal    = false;
    this->managed     = true;
    this->context     = NULL;
    this->relOffset   = 0;
    this->ambientSP   = NULL;


    // Method associated w/a stub will never have a JitManager.
    this->pIJM        = NULL;
    this->MethodToken = NULL;
    this->currentAppDomain      = pCF->GetAppDomain();
    this->exactGenericArgsToken = NULL;

    this->chainReason    = CHAIN_NONE;
    this->eStubFrameType = type;

#ifdef _DEBUG
    // After we just init it, it had better be valid.
    this->AssertValid();
#endif
}

//-----------------------------------------------------------------------------
// Initialize a FrameInfo to be used for an "InternalFrame"
// Frame should be a derived class of FramedMethodFrame.
// FrameInfo's MethodDesc will be for managed wrapper for native call.
//-----------------------------------------------------------------------------
void FrameInfo::InitForM2UInternalFrame(CrawlFrame * pCF)
{
    // For a M2U call, there's a managed method wrapping the unmanaged call. Use that.
    Frame * pFrame = pCF->GetFrame();
    _ASSERTE(pFrame->GetTransitionType() == Frame::TT_M2U);
    FramedMethodFrame * pM2U = static_cast<FramedMethodFrame*> (pFrame);
    MethodDesc * pMDWrapper = pM2U->GetFunction();

    // Soem M2U transitions may not have a function associated w/ them,
    // so pMDWrapper may be NULL. PInvokeCalliFrame is an example.

    InitFromStubHelper(pCF, pMDWrapper, STUBFRAME_M2U);
    InitForScratchFrameInfo();
}

//-----------------------------------------------------------------------------
// Initialize for the U2M case...
//-----------------------------------------------------------------------------
void FrameInfo::InitForU2MInternalFrame(CrawlFrame * pCF)
{
    PREFIX_ASSUME(pCF != NULL);
    MethodDesc * pMDHint = NULL;


    InitFromStubHelper(pCF, pMDHint, STUBFRAME_U2M);
    InitForScratchFrameInfo();
}

//-----------------------------------------------------------------------------
// Init for an possible internal call
// Returns true if used, else false.
//-----------------------------------------------------------------------------
bool FrameInfo::InitForPossibleInternalCall(CrawlFrame * pCF)
{
    Frame * pFrame;
    pFrame = pCF->GetFrame();
    _ASSERTE(pFrame->GetTransitionType() == Frame::TT_InternalCall);

    MethodDesc * pMDWrapper = pFrame->GetFunction(); // May be null

    if (pMDWrapper == NULL)
    {
        return false;
    }

    InitFromStubHelper(pCF, pMDWrapper, STUBFRAME_INTERNALCALL);
    return true;
}


//-----------------------------------------------------------------------------
// Init for an AD transition
//-----------------------------------------------------------------------------
void FrameInfo::InitForADTransition(CrawlFrame * pCF)
{
    Frame * pFrame;
    pFrame = pCF->GetFrame();
    _ASSERTE(pFrame->GetTransitionType() == Frame::TT_AppDomain);
    MethodDesc * pMDWrapper = NULL;

    InitFromStubHelper(pCF, pMDWrapper, STUBFRAME_APPDOMAIN_TRANSITION);
    InitForScratchFrameInfo();
}


//-----------------------------------------------------------------------------
// Init frame for a dynamic method.
//-----------------------------------------------------------------------------
void FrameInfo::InitForDynamicMethod(CrawlFrame * pCF)
{
    // These are just stack markers that there's a dynamic method on the callstack.
    InitFromStubHelper(pCF, NULL, STUBFRAME_LIGHTWEIGHT_FUNCTION);
    // Do not call InitForScratchFrameInfo() here!  Please refer to the comment in that function.
}

//-----------------------------------------------------------------------------
// Init an internal frame to mark a func-eval.
//-----------------------------------------------------------------------------
void FrameInfo::InitForFuncEval(CrawlFrame * pCF)
{
    // We don't store a MethodDesc hint referring to the method we're going to invoke because
    // uses of stub frames will assume the MD is relative to the AppDomain the frame is in.
    // For cross-AD funcevals, we're invoking a method in a domain other than the one this frame
    // is in.
    MethodDesc * pMDHint = NULL;

    // These are just stack markers that there's a dynamic method on the callstack.
    InitFromStubHelper(pCF, pMDHint, STUBFRAME_FUNC_EVAL);
    InitForScratchFrameInfo();
}


//-----------------------------------------------------------------------------
// Initialize a FrameInfo for sending the CHAIN_THREAD_START reason.
//-----------------------------------------------------------------------------
void FrameInfo::InitForThreadStart(bool fManaged, REGDISPLAY * pRDSrc)
{
    this->frame = (Frame *) FRAME_TOP;
    this->md = NULL;
    CopyREGDISPLAY(&(this->registers), pRDSrc);
    this->fp    = ROOT_MOST_FRAME;

    this->quickUnwind = false;
    this->internal = false;
    this->managed     = fManaged;
    this->context     = NULL;
    this->relOffset   = 0;
    this->pIJM        = NULL;
    this->MethodToken = NULL;

    this->currentAppDomain = NULL;
    this->exactGenericArgsToken = NULL;


    this->chainReason    = CHAIN_THREAD_START;
    this->eStubFrameType = STUBFRAME_NONE;

#ifdef _DEBUG
    // After we just init it, it had better be valid.
    this->AssertValid();
#endif
}

//-----------------------------------------------------------------------------
// Initialize a FrameInfo for sending a CHAIN_ENTER_MANAGED.
//-----------------------------------------------------------------------------
void FrameInfo::InitForEnterManagedChain(FramePointer fpRoot)
{
    // Nobody should use a EnterManagedChain's Frame*, but there's no
    // good value to enforce that.
    this->frame = (Frame *) FRAME_TOP;
    this->md    = NULL;
    memset((void *)&this->registers, 0, sizeof(this->registers));
    this->fp = fpRoot;

    this->quickUnwind = true;
    this->internal    = false;
    this->managed     = true;
    this->context     = NULL;
    this->relOffset   = 0;
    this->pIJM        = NULL;
    this->MethodToken = NULL;

    this->currentAppDomain = NULL;
    this->exactGenericArgsToken = NULL;


    this->chainReason    = CHAIN_ENTER_MANAGED;
    this->eStubFrameType = STUBFRAME_NONE;
}

StackWalkAction TrackUMChain(CrawlFrame *pCF, DebuggerFrameData *d)
{
    Frame *frame = g_pEEInterface->GetFrame(pCF);

    // If we encounter an ExitFrame out in the wild, then we'll convert it to an UM chain.
    if (!d->IsTrackingUMChain())
    {
        if ((frame != NULL) && (frame != FRAME_TOP) && (frame->GetFrameType() == Frame::TYPE_EXIT))
        {
            LOG((LF_CORDB, LL_EVERYTHING, "DWSP. ExitFrame while not tracking\n"));
            REGDISPLAY* pRDSrc = pCF->GetRegisterSet();

            d->BeginTrackingUMChain(GetSP(pRDSrc), pRDSrc);

            // fall through and we'll send the UM chain.
        }
        else
        {
            return SWA_CONTINUE;
        }
    }

    _ASSERTE(d->IsTrackingUMChain());


    // If we're tracking an UM chain, then we need to:
    // - possibly refine the start & end values as we get new information in the stacktrace.
    // - possibly cancel the UM chain for various heuristics.
    // - possibly dispatch if we've hit managed code again.

    bool fDispatchUMChain = false;
    // UM Chain stops when managed code starts again.
    if (frame != NULL)
    {
        // If it's just a EE Frame, then update this as a possible end of stack range for the UM chain.
        d->SetUMChainEnd(FramePointer::MakeFramePointer((LPVOID)(frame)
                                                        IA64_ARG(LEAF_MOST_FRAME_BSP)));


        Frame::ETransitionType t = frame->GetTransitionType();
        int ft      = frame->GetFrameType();


        if ((t == Frame::TT_AppDomain) || (ft == Frame::TYPE_FUNC_EVAL))
        {
            d->CancelUMChain();
            return SWA_CONTINUE;
        }

        // If we hit an M2U frame, then go ahead and dispatch the UM chain now.
        // This will likely also be an exit frame.
        if (t == Frame::TT_M2U)
        {
            fDispatchUMChain = true;
        }

        // If we get an Exit frame, we can use that to "prune" the UM chain to a more friendly state.
        // This heuristic is optional, it just eliminates lots of internal mscorwks frames from the callstack.
        if (ft == Frame::TYPE_EXIT)
        {
            // If we have a valid reg-display (non-null IP) then update it.
            // We may have an invalid reg-display if we have an exit frame on an inactive thread.
            REGDISPLAY * pNewRD = pCF->GetRegisterSet();
            if (GetControlPC(pNewRD) != NULL)
            {
                LOG((LF_CORDB, LL_EVERYTHING, "DWSP. updating RD while tracking UM chain\n"));
                CopyREGDISPLAY(d->GetUMChainStartRD(), pNewRD);
            }

            FramePointer fpLeaf = GetSP(d->GetUMChainStartRD());
            _ASSERTE(IsCloserToLeaf(fpLeaf, d->GetUMChainEnd()));


            _ASSERTE(!d->fHitExitFrame); // should only have 1 exit frame per UM chain code.
            d->fHitExitFrame = true;

            FramePointer potentialFP;

            FramePointer fpNewChainEnd = d->GetUMChainEnd();

            // Check to see if we are inside the unmanaged call. We want to make sure we only report an exit frame after
            // we've really exited. There is a short period between where we setup the frame and when we actually exit
            // the runtime. This check is intended to ensure we're actually outside now.
            if (HasExitRuntime(frame, d, &potentialFP))
            {
                LOG((LF_CORDB, LL_EVERYTHING, "HasExitRuntime. potentialFP=0x%p\n", potentialFP.GetSPValue()));
                // If we have no call site, manufacture a FP
                // using the current frame.

                if (potentialFP == LEAF_MOST_FRAME)
                {
                    fpNewChainEnd = FramePointer::MakeFramePointer((LPVOID)((BYTE*)frame - sizeof(LPVOID))
                                                                   IA64_ARG(LEAF_MOST_FRAME_BSP));
                }
                else
                {
                    fpNewChainEnd = potentialFP;
                }

            }

            fDispatchUMChain = true;

            // If we got a valid chain end, then prune the UM chain accordingly.
            // Note that some EE Frames will give invalid info back so we have to check.
            // PInvokeCalliFrame is one example (when doing MC++ function pointers)
            if (IsCloserToRoot(fpNewChainEnd, fpLeaf))
            {
                d->SetUMChainEnd(fpNewChainEnd);
            }
            else
            {
                _ASSERTE(IsCloserToLeaf(fpLeaf, d->GetUMChainEnd()));
            }
        } // end ExitFrame

        // Only CLR internal code / stubs can push Frames onto the Frame chain.
        // So if we hit a raw interceptor frame before we hit any managed frame, then this whole
        // UM chain must still be in CLR internal code.
        // Either way, this UM chain has ended (and some new chain based off the frame has started)
        // so we need to either Cancel the chain or dispatch it.
        if (frame->GetInterception() != Frame::INTERCEPTION_NONE)
        {
            fDispatchUMChain = true;
  
            //d->CancelUMChain();
            //return SWA_CONTINUE;
        }
    }
    else
    {
        // If it's a real method (not just an EE Frame), then the UM chain is over.
        fDispatchUMChain = true;
    }


    if (fDispatchUMChain)
    {
        // Check if we should cancel the UM chain.

        // We need to discriminate between the following 2 cases:
        // 1) Managed -(a)-> mscorwks -(b)-> Managed  (leaf)
        // 2) Native  -(a)-> mscorwks -(b)-> Managed  (leaf)
        // Case 1 could happen if a managed call injects a stub (such as w/ delegates).
        // In both cases, the (mscorwks-(b)->managed) transition causes a IsNativeMarker callback
        // which initiates a UM chain. In case 1, we wan't to cancel the UM chain, but
        // in case 2 we want to dispatch it.
        // The difference is case #2 will have some EE Frame at (b) and case #1 won't.
        // That EE Frame should have caused us to dispatch the call for the managed method, and
        // thus by the time we get around to dispatching the UM Chain, we shouldn't have a managed
        // method waiting to be dispatched in the DebuggerFrameData.

        if (d->needParentInfo && d->info.HasMethodFrame())
        {
            LOG((LF_CORDB, LL_EVERYTHING, "Cancelling UM Chain b/c it's internal\n"));
            d->CancelUMChain();
            return SWA_CONTINUE;
        }

        if (!d->fHitExitFrame && !d->ShouldIgnoreNonmethodFrames() && !d->IsLeafCallback())
        {
            LOG((LF_CORDB, LL_EVERYTHING, "Cancelling UM Chain b/c it's stepper not requested\n"));
            d->CancelUMChain();
            return SWA_CONTINUE;
        }


        // Ok, we haven't cancelled it yet, so go ahead and send the UM chain.
        FrameInfo f;
        FramePointer fpRoot = d->GetUMChainEnd();
        FramePointer fpLeaf = GetSP(d->GetUMChainStartRD());

        // If we didn't actually get any range, then don't bother sending it.
        if (fpRoot == fpLeaf)
        {
            d->CancelUMChain();
            return SWA_CONTINUE;
        }

        f.InitForUMChain(fpRoot, d->GetUMChainStartRD());

        if (d->InvokeCallback(&f) == SWA_ABORT)
        {
            // don't need to cancel if they abort.
            return SWA_ABORT;
        }
        d->CancelUMChain(); // now that we've sent it, we're done.


        // Check for a M2U internal frame.
        if (d->ShouldProvideInternalFrames() && (frame != NULL) && (frame != FRAME_TOP))
        {
            // We want to dispatch a M2U transition right after we dispatch the UM chain.
            Frame::ETransitionType t = frame->GetTransitionType();
            if (t == Frame::TT_M2U)
            {
                // Frame for a M2U transition.
                FrameInfo fM2U;
                fM2U.InitForM2UInternalFrame(pCF);
                if (d->InvokeCallback(&fM2U) == SWA_ABORT)
                {
                    return SWA_ABORT;
                }
            }
        }


    }

    return SWA_CONTINUE;
}

FramePointer GetFramePointerForDebugger(DebuggerFrameData* pData, CrawlFrame* pCF)
{
    FramePointer fpResult;

    if ((pCF == NULL || !pCF->IsFrameless()) && pData->info.frame != NULL)
    {
        //
        // If we're in an explicit frame now, and the previous frame was
        // also an explicit frame, pPC will not have been updated.  So
        // use the address of the frame itself as fp.
        //
        fpResult = FramePointer::MakeFramePointer((LPVOID)(pData->info.frame));

        LOG((LF_CORDB, LL_INFO100000, "GFPFD: Two explicit frames in a row; using frame address 0x%p\n",
             pData->info.frame));
    }
    else
    {
        //
        // Otherwise use pPC as the frame pointer, as this will be
        // pointing to the return address on the stack.
        //
        fpResult = FramePointer::MakeFramePointer((LPVOID)GetRegdisplayStackMark(&(pData->regDisplay)));
    }



    LOG((LF_CORDB, LL_INFO100000, "GFPFD: Frame pointer is 0x%p\n", fpResult.GetSPValue()));

    return fpResult;
}




//-----------------------------------------------------------------------------
// StackWalkAction DebuggerWalkStackProc():  This is the callback called
// by the EE.
// Note that since we don't know what the frame pointer for frame
// X is until we've looked at the caller of frame X, we actually end up
// stashing the info and pData pointers in the DebuggerFrameDat struct, and
// then invoking pCallback when we've moved up one level, into the caller's
// frame.  We use the needParentInfo field to indicate that the previous frame
// needed this (parental) info, and so when it's true we should invoke
// pCallback.
// What happens is this: if the previous frame set needParentInfo, then we
// do pCallback (and set needParentInfo to false).
// Then we look at the current frame - if it's frameless (ie,
// managed), then we set needParentInfo to callback in the next frame.
// Otherwise we must be at a chain boundary, and so we set the chain reason
// appropriately.  We then figure out what type of frame it is, setting
// flags depending on the type.  If the user should see this frame, then
// we'll set needParentInfo to record it's existence.  Lastly, if we're in
// a funky frame, we'll explicitly update the register set, since the
// CrawlFrame doesn't do it automatically.
//-----------------------------------------------------------------------------
StackWalkAction DebuggerWalkStackProc(CrawlFrame *pCF, void *data)
{
    DebuggerFrameData *d = (DebuggerFrameData *)data;

    if (pCF->IsNativeMarker())
    {

        REGDISPLAY* pRDSrc = pCF->GetRegisterSet();
        d->BeginTrackingUMChain(GetSP(pRDSrc), pRDSrc);

        return SWA_CONTINUE;
    }

    // Note that a CrawlFrame may have both a methoddesc & an EE Frame.
    Frame *frame = g_pEEInterface->GetFrame(pCF);
    MethodDesc *md = pCF->GetFunction();

    LOG((LF_CORDB, LL_EVERYTHING, "Calling DebuggerWalkStackProc. Frame=0x%p, md=0x%p(%s), native_marker=%d\n",
        frame, md, (md == NULL || md == (MethodDesc*)POISONC) ? "null" : md->m_pszDebugMethodName, pCF->IsNativeMarker() ));




    // The fp for a frame must be obtained from the
    // _next_ frame. Fill it in now for the previous frame, if
    // appropriate.
    //

    if (d->needParentInfo)
    {
        LOG((LF_CORDB, LL_INFO100000, "DWSP: NeedParentInfo.\n"));

        d->info.fp = GetFramePointerForDebugger(d, pCF);

#if defined(_DEBUG)
        _ASSERTE(IsCloserToLeaf(d->previousFP, d->info.fp));
        d->previousFP = d->info.fp;
#endif // _DEBUG

        d->needParentInfo = false;

        {
            // Don't invoke Stubs if we're not asking for internal frames.
            bool fDoInvoke = true;
            if (!d->ShouldProvideInternalFrames())
            {
                if (d->info.HasStubFrame())
                {
                    fDoInvoke = false;
                }
            }

            LOG((LF_CORDB, LL_INFO1000000, "DWSP: handling our target\n"));

            if (fDoInvoke)
            {
                if (d->InvokeCallback(&d->info) == SWA_ABORT)
                {
                    return SWA_ABORT;
                }
            }

            d->info.eStubFrameType = STUBFRAME_NONE;
        }
    } // if (d->needParentInfo)


    {

        // Track the UM chain after we flush any managed goo from the last iteration.
        if (TrackUMChain(pCF, d) == SWA_ABORT)
        {
            return SWA_ABORT;
        }
    }


    // Track if we want to send a callback for this Frame / Method
    bool use=false;

    //
    // Examine the frame.
    //

    // We assume that the stack walker is just updating the
    // register display we passed in - assert it to be sure
    _ASSERTE(pCF->GetRegisterSet() == &d->regDisplay);


    d->info.frame = frame;
    d->info.ambientSP = NULL;

    // Record the appdomain that the thread was in when it
    // was running code for this frame.
    d->info.currentAppDomain = pCF->GetAppDomain();

    //  Grab all the info from CrawlFrame that we need to
    //  check for "Am I in an exeption code blob?" now.



    // For frames w/o method data, send them as an internal stub frame.
    if ((md != NULL) && md->IsDynamicMethod())
    {
        // Only Send the frame if "InternalFrames" are requested.
        // Else completely ignore it.
        if (d->ShouldProvideInternalFrames())
        {
            d->info.InitForDynamicMethod(pCF);

            // We'll loop around to get the FramePointer. Only modification to FrameInfo
            // after this is filling in framepointer and resetting MD.
            use = true;
        }
    }
    else if (pCF->IsFrameless())
    {
        // Regular managed-method.
        LOG((LF_CORDB, LL_INFO100000, "DWSP: Is frameless.\n"));
        use = true;
        d->info.managed = true;
        d->info.internal = false;
        d->info.chainReason = CHAIN_NONE;
        d->needParentInfo = true; // Possibly need chain reason
        d->info.relOffset =  ADJUST_REL_OFFSET(pCF, d->info.fIsLeaf);
        d->info.pIJM = pCF->GetJitManager();
        d->info.MethodToken = pCF->GetMethodToken();

#ifdef _X86_
        // This is collecting the ambientSP a lot more than we actually need it. Only time we need it is
        // inspecting local vars that are based off the ambient esp.
        d->info.ambientSP = pCF->GetAmbientSPFromCrawlFrame();
#endif
    }
    else
    {
        d->info.pIJM = NULL;
        d->info.MethodToken = NULL;
        //
        // Retrieve any interception info
        //

        switch (frame->GetInterception())
        {
        case Frame::INTERCEPTION_CLASS_INIT:
            d->info.chainReason = CHAIN_CLASS_INIT;
            break;

        case Frame::INTERCEPTION_EXCEPTION:
            d->info.chainReason = CHAIN_EXCEPTION_FILTER;
            break;

        case Frame::INTERCEPTION_CONTEXT:
            d->info.chainReason = CHAIN_CONTEXT_POLICY;
            break;

        case Frame::INTERCEPTION_SECURITY:
            d->info.chainReason = CHAIN_SECURITY;
            break;

        default:
            d->info.chainReason = CHAIN_NONE;
        }

        //
        // Look at the frame type to figure out how to treat it.
        //

        LOG((LF_CORDB, LL_INFO100000, "DWSP: Chain reason is 0x%X.\n", d->info.chainReason));

        switch (frame->GetFrameType())
        {
        case Frame::TYPE_ENTRY: // We now ignore entry + exit frames.
        case Frame::TYPE_EXIT:
        case Frame::TYPE_INTERNAL:

            /* If we have a specific interception type, use it. However, if this
               is the top-most frame (with a specific type), we can ignore it
               and it wont appear in the stack-trace */
#define INTERNAL_FRAME_ACTION(d, use)                              \
    (d)->info.managed = true;       \
    (d)->info.internal = false;     \
    use = true

            LOG((LF_CORDB, LL_INFO100000, "DWSP: Frame type is TYPE_INTERNAL.\n"));
            if (d->info.chainReason == CHAIN_NONE || pCF->IsActiveFrame())
            {
                use = false;
            }
            else
            {
                INTERNAL_FRAME_ACTION(d, use);
            }
            break;

        case Frame::TYPE_INTERCEPTION:
        case Frame::TYPE_SECURITY: // Security is a sub-type of interception
            LOG((LF_CORDB, LL_INFO100000, "DWSP: Frame type is TYPE_INTERCEPTION/TYPE_SECURITY.\n"));
            d->info.managed = true;
            d->info.internal = true;
            use = true;
            break;

        case Frame::TYPE_CALL:
            LOG((LF_CORDB, LL_INFO100000, "DWSP: Frame type is TYPE_CALL.\n"));
            d->info.managed = true;
            d->info.internal = false;
            use = true;
            break;

        case Frame::TYPE_FUNC_EVAL:
            LOG((LF_CORDB, LL_INFO100000, "DWSP: Frame type is TYPE_FUNC_EVAL.\n"));
            d->info.managed = true;
            d->info.internal = true;
            d->info.chainReason = CHAIN_FUNC_EVAL;

            {
                FuncEvalFrame *pFuncEvalFrame = static_cast<FuncEvalFrame *>(frame);
                use = pFuncEvalFrame->ShowFrame() ? true : false;
            }

            // Send Internal frame. This is "inside" (leafmost) the chain, so we send it first
            // since sending starts from the leaf.
            if (use && d->ShouldProvideInternalFrames())
            {
                FrameInfo f;
                f.InitForFuncEval(pCF);
                if (d->InvokeCallback(&f) == SWA_ABORT)
                {
                    return SWA_ABORT;
                }
            }

            break;

        // Put frames we want to ignore here:
        case Frame::TYPE_MULTICAST:
            LOG((LF_CORDB, LL_INFO100000, "DWSP: Frame type is TYPE_MULTICAST.\n"));
            if (d->ShouldIgnoreNonmethodFrames())
            {
                // Multicast frames exist only to gc protect the arguments
                // between invocations of a delegate.  They don't have code that
                // we can (currently) show the user (we could change this with
                // work, but why bother?  It's an internal stub, and even if the
                // user could see it, they can't modify it).
                LOG((LF_CORDB, LL_INFO100000, "DWSP: Skipping frame 0x%x b/c it's "
                    "a multicast frame!\n", frame));
                use = false;
            }
            else
            {
                LOG((LF_CORDB, LL_INFO100000, "DWSP: NOT Skipping frame 0x%x even thought it's "
                    "a multicast frame!\n", frame));
                INTERNAL_FRAME_ACTION(d, use);
            }
            break;

        case Frame::TYPE_TP_METHOD_FRAME:
            LOG((LF_CORDB, LL_INFO100000, "DWSP: Frame type is TYPE_TP_METHOD_FRAME.\n"));
            if (d->ShouldIgnoreNonmethodFrames())
            {
                // Transparant Proxies push a frame onto the stack that they
                // use to figure out where they're really going; this frame
                // doesn't actually contain any code, although it does have
                // enough info into fooling our routines into thinking it does:
                // Just ignore these.
                LOG((LF_CORDB, LL_INFO100000, "DWSP: Skipping frame 0x%x b/c it's "
                    "a transparant proxy frame!\n", frame));
                use = false;
            }
            else
            {
                // Otherwise do the same thing as for internal frames
                LOG((LF_CORDB, LL_INFO100000, "DWSP: NOT Skipping frame 0x%x even though it's "
                    "a transparant proxy frame!\n", frame));
                INTERNAL_FRAME_ACTION(d, use);
            }
            break;

        default:
            _ASSERTE(!"Invalid frame type!");
            break;
        }
    }


    // Check for ICorDebugInternalFrame stuff.
    // These callbacks are dispatched out of band.
    if (d->ShouldProvideInternalFrames() && (frame != NULL) && (frame != FRAME_TOP))
    {
        // We want to dispatcha M2U transition right after we dispatch the UM chain.
        Frame::ETransitionType t = frame->GetTransitionType();
        FrameInfo f;
        bool fUse = false;

        // We don't want to tell the RS about ADs that haven't been published yet.
        // We know for sure the AD is published once the RS asks usto attach to it, and thus calls AttachDebuggerToAppDomain.
        // That will in turn twiddle these bits on the AD mask.
        bool fIsAppDomainPublished = ((pCF->GetAppDomain()->GetDebuggerAttached() & AppDomain::DEBUGGER_ATTACHING) != 0);

        if (t == Frame::TT_U2M)
        {

            f.InitForU2MInternalFrame(pCF);
            fUse = true;
        }
        else if (t == Frame::TT_AppDomain)
        {
            if (fIsAppDomainPublished)
            {
                // Internal frame for an Appdomain transition.
                // If the frame is for an AD that has't yet been published to the debugger, ignore it.
                f.InitForADTransition(pCF);
                fUse = true;
            }
        }

        // Frame's setup. Now invoke the callback.
        if (fUse)
        {
            if (d->InvokeCallback(&f) == SWA_ABORT)
            {
                return SWA_ABORT;
            }
        }
    } // should we give frames?



    if (use)
    {
        //
        // If we are returning a complete stack walk from the helper thread, then we
        // need to gather information to instantiate generics.  However, a stepper doing
        // a stackwalk does not need this information, so skip in that case.
        //
        if (d->ShouldIgnoreNonmethodFrames())
        {
            // Finding sizes of value types on the argument stack while
            // looking for the arg runs the class loader in non-load mode.
            ENABLE_FORBID_GC_LOADER_USE_IN_THIS_SCOPE();
            d->info.exactGenericArgsToken = pCF->GetExactGenericArgsToken();
        }
        else
        {
            d->info.exactGenericArgsToken = NULL;
        }

        d->info.md = md;
        CopyREGDISPLAY(&(d->info.registers), &(d->regDisplay));



        d->needParentInfo = true;
        LOG((LF_CORDB, LL_INFO100000, "DWSP: Setting needParentInfo\n"));
    }


    //
    // The stackwalker doesn't update the register set for the
    // case where a non-frameless frame is returning to another
    // non-frameless frame.  Cover this case.
    //
    // !!! This assumes that updating the register set multiple times
    // for a given frame times is not a bad thing...
    //
    if (!pCF->IsFrameless())
    {
        LOG((LF_CORDB, LL_INFO100000, "DWSP: updating regdisplay.\n"));
        pCF->GetFrame()->UpdateRegDisplay(&d->regDisplay);
    }

    return SWA_CONTINUE;
}

#if defined(_X86_)
// Helper to get the Wait-Sleep-Join bit from the thread
bool IsInWaitSleepJoin(Thread * pThread)
{
    // Partial User state is sufficient because that has the bit we're checking against.
    CorDebugUserState cts = g_pEEInterface->GetPartialUserState(pThread);
    return ((cts & USER_WAIT_SLEEP_JOIN) != 0);
}

//-----------------------------------------------------------------------------
// Decide if we should send an UM leaf chain.
// This geoes through a bunch of heuristics.
// The driving guidelines here are:
// - we try not to send an UM chain if it's just internal mscorwks stuff
//   and we know it can't have native user code.
//   (ex, anything beyond a filter context, various hijacks, etc).
// - If it may have native user code, we send it anyway.
//-----------------------------------------------------------------------------
bool ShouldSendUMLeafChain(Thread * pThread)
{
    if (g_fProcessDetach)
    {
        return false;
    }

    if (pThread->IsUnstarted() || pThread->IsDead())
    {
        return false;
    }

    // If a thread is suspended for sync purposes, it was suspended from managed
    // code and the only native code is a mscorwks hijack.
    // There are a few caveats here:
    // - This means a thread will lose it's UM chain. But what if a user inactive thread
    // enters the CLR from native code and hits a GC toggle? We'll lose that entire
    // UM chain.
    // - at a managed-only stop, preemptive threads are still live. Thus a thread
    // may not have this state set, run a little, try to enter the GC, and then get
    // this state set. Thus we'll lose the UM chain right out from under our noses.
    Thread::ThreadState ts = pThread->GetSnapshotState();
    if ((ts & Thread::TS_SyncSuspended) != 0)
    {

        return false;
    }


    if (IsInWaitSleepJoin(pThread))
    {
        return false;
    }

    // If we're tracing ourselves, we must be in managed code.
    // Native user code can't initiate a managed stackwalk.
    if (pThread == GetThread())
    {
        return false;
    }

    return true;
}

//-----------------------------------------------------------------------------
// Prepare a Leaf UM chain. This assumes we should send an UM leaf chain.
// Returns true if we actually prep for an UM leaf,
// false if we don't.
//-----------------------------------------------------------------------------
bool PrepareLeafUMChain(DebuggerFrameData * pData, CONTEXT * pCtxTemp)
{
    // Get the current user context (depends on if we're the active thread or not).
    Thread * thread = pData->GetThread();
    REGDISPLAY * pRDSrc = NULL;
    REGDISPLAY rdTemp;


#ifdef _DEBUG
    // Anybody stopped at an native debug event (and hijacked) should have a filter ctx.
    if (thread->GetInteropDebuggingHijacked() && (thread->GetFrame() != NULL) && (thread->GetFrame() != FRAME_TOP))
    {
        _ASSERTE(g_pEEInterface->GetThreadFilterContext(thread) != NULL);
    }
#endif

    // If we're hijacked, then we assume we're in native code. This covers the active thread case.
    if (g_pEEInterface->GetThreadFilterContext(thread) != NULL)
    {
        LOG((LF_CORDB, LL_EVERYTHING, "DWS - sending special case UM Chain.\n"));

        // This will get it from the filter ctx.
        pRDSrc = &(pData->regDisplay);
    }
    else
    {
        // For inactive thread, we may not be hijacked. So just get the current ctx.
        // This will use a filter ctx if we have one.
        // We may suspend a thread in native code w/o hijacking it, so it's still at it's live context.
        // This can happen when we get a debug event on 1 thread; and then switch to look at another thread.
        // This is very common when debugging apps w/ cross-thread causality (including COM STA objects)
        pRDSrc = &rdTemp;

        bool fOk;


        // We need to get thread's context (InitRegDisplay will do that under the covers).
        // If this is our thread, we're in bad shape. Fortunately that should never happen.
        _ASSERTE(thread != GetThread());

        Thread::SuspendThreadResult str = thread->SuspendThread();
        if (str != Thread::STR_Success)
        {
            return false;
        }

        fOk = g_pEEInterface->InitRegDisplay(thread, pRDSrc, pCtxTemp, false);
        thread->ResumeThread();

        if (!fOk)
        {
            return false;
        }
    }

    // By now we have a Regdisplay from somewhere (filter ctx, current ctx, etc).
    _ASSERTE(pRDSrc != NULL);

    // If we're stopped in mscorwks (b/c of a handler for a managed BP), then the filter ctx will
    // still be set out in jitted code.
    // If our regdisplay is out in UM code , then send a UM chain.
    BYTE* ip = (BYTE*) GetControlPC(pRDSrc);
    if (g_pEEInterface->IsManagedNativeCode(ip))
    {
        return false;
    }

    LOG((LF_CORDB, LL_EVERYTHING, "DWS - sending leaf UM Chain.\n"));

    // Get the ending fp. We may not have any managed goo on the stack (eg, native thread called
    // into a managed method and then returned from it).
    FramePointer fpRoot;
    Frame * pFrame = thread->GetFrame();
    if ((pFrame != NULL) && (pFrame != FRAME_TOP))
    {
        fpRoot = FramePointer::MakeFramePointer((void*) pFrame);
    }
    else
    {
        fpRoot= ROOT_MOST_FRAME;
    }


    // Start tracking an UM chain. We won't actually send the UM chain until
    // we hit managed code. Since this is the leaf, we don't need to send an
    // Enter-Managed chain either.
    pData->BeginTrackingUMChain(fpRoot, pRDSrc);

    return true;
}
#endif

//-----------------------------------------------------------------------------
// Entry function for the debugger's stackwalking layer.
// This will invoke pCallback(FrameInfo * pInfo, pData) for each 'frame'
//-----------------------------------------------------------------------------
StackWalkAction DebuggerWalkStack(Thread *thread,
                                  FramePointer targetFP,
                                  CONTEXT *context,
                                  BOOL contextValid,
                                  DebuggerStackCallback pCallback,
                                  void *pData,
                                  BOOL fIgnoreNonmethodFrames,
                                  IpcTarget iWhich)
{
    _ASSERTE(context != NULL);

    DebuggerFrameData data;

    StackWalkAction result = SWA_CONTINUE;
    bool fRegInit = false;

    LOG((LF_CORDB, LL_EVERYTHING, "DebuggerWalkStack called\n"));
    // For the in-process case, we need to be able to handle a thread trying to trace itself.

    if(contextValid || g_pEEInterface->GetThreadFilterContext(thread) != NULL)
    {
        fRegInit = g_pEEInterface->InitRegDisplay(thread, &data.regDisplay, context, contextValid != 0);

        // This should only have the possiblilty of failing on Win9x
        _ASSERTE(fRegInit || RunningOnWin95());
    }

    if (!fRegInit)
    {
#if defined(CONTEXT_EXTENDED_REGISTERS)

            // Note: the size of a CONTEXT record contains the extended registers, but the context pointer we're given
            // here may not have room for them. Therefore, we only set the non-extended part of the context to 0.
            memset((void *)context, 0, offsetof(CONTEXT, ExtendedRegisters));
#else
            memset((void *)context, 0, sizeof(CONTEXT));
#endif
            memset((void *)&data, 0, sizeof(data));

#if defined(_X86_)
            data.regDisplay.pPC = (SLOT*)&(context->Eip);
            data.regDisplay.PCTAddr = (TADDR)&(context->Eip);

#elif defined(_PPC_)
            data.regDisplay.pPC = (SLOT*)&(context->Iar);

#else
            PORTABILITY_ASSERT("DebuggerWalkStack needs extended register information on this platform.");

#endif
    }

    data.Init(thread, targetFP, fIgnoreNonmethodFrames, pCallback, pData);


#if defined(_X86_)
    CONTEXT ctxTemp; // Temp context for Leaf UM chain. Need it here so that it stays alive for whole stackwalk.

    // Important case for Interop Debugging -
    // We may be stopped in Native Code (perhaps at a BP) w/ no Transition frame on the stack!
    // We still need to send an UM Chain for this case.
    if (ShouldSendUMLeafChain(thread))
    {
        // It's possible this may fail (eg, GetContext fails on win9x), so we're not guaranteed
        // to be sending an UM chain even though we want to.
        PrepareLeafUMChain(&data, &ctxTemp);

    }


#endif // x86

    if ((result != SWA_FAILED) && !thread->IsUnstarted() && !thread->IsDead())
    {
        int flags = 0;

        result = g_pEEInterface->StackWalkFramesEx(thread, &data.regDisplay,
                                                   DebuggerWalkStackProc,
                                                   &data, flags | HANDLESKIPPEDFRAMES | NOTIFY_ON_U2M_TRANSITIONS | ALLOW_ASYNC_STACK_WALK);
    }
    else
    {
        result = SWA_DONE;
    }

    if (result == SWA_DONE || result == SWA_FAILED) // SWA_FAILED if no frames
    {
        // Since Debugger StackWalk callbacks are delayed 1 frame from EE stackwalk callbacks, we
        // have to touch up the 1 leftover here.
        if (data.needParentInfo)
        {
            data.info.fp = GetFramePointerForDebugger(&data, NULL);

            if (data.InvokeCallback(&data.info) == SWA_ABORT)
            {
                return SWA_ABORT;
            }
        }

        //
        // Top off the stack trace as necessary w/ a thread-start chain.
        //
        if (data.IsTrackingUMChain())
        {
            // This is the common case b/c managed code gets called from native code.
            data.info.InitForThreadStart(false, data.GetUMChainStartRD());
        }
        else
        {
            // This is the rare case.
            data.info.InitForThreadStart(true, &(data.regDisplay));
        }
        result = data.InvokeCallback(&data.info);
    }
    return result;
}
