/**CFile****************************************************************

  FileName    [retIncrem.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Retiming package.]

  Synopsis    [Incremental retiming in one direction.]

  Author      [Alan Mishchenko]
  
  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - Oct 31, 2006.]

  Revision    [$Id: retIncrem.c,v 1.00 2006/10/31 00:00:00 alanmi Exp $]

***********************************************************************/

#include "retInt.h"

////////////////////////////////////////////////////////////////////////
///                        DECLARATIONS                              ///
////////////////////////////////////////////////////////////////////////

static st_table * Abc_NtkRetimePrepareLatches( Abc_Ntk_t * pNtk );
static int Abc_NtkRetimeFinalizeLatches( Abc_Ntk_t * pNtk, st_table * tLatches, int nIdMaxStart );
static int Abc_NtkRetimeOneWay( Abc_Ntk_t * pNtk, int fForward, int fVerbose );

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    [Performs retiming in one direction.]

  Description [Currently does not retime over black boxes.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int Abc_NtkRetimeIncremental( Abc_Ntk_t * pNtk, int fForward, int fMinDelay, int fVerbose )
{
    Abc_Ntk_t * pNtkCopy = NULL;
    Vec_Ptr_t * vBoxes;
    st_table * tLatches;
    int nLatches = Abc_NtkLatchNum(pNtk);
    int nIdMaxStart = Abc_NtkObjNumMax(pNtk);
    int RetValue, nIterLimit;
    if ( Abc_NtkNodeNum(pNtk) == 0 )
        return 0;
    // reorder CI/CO/latch inputs
    Abc_NtkOrderCisCos( pNtk );
    if ( fMinDelay ) 
    {
        nIterLimit = 2 * Abc_NtkGetLevelNum(pNtk);
        pNtkCopy = Abc_NtkDup( pNtk );
        tLatches = Abc_NtkRetimePrepareLatches( pNtkCopy );
        st_free_table( tLatches );
    }
    // collect latches and remove CIs/COs
    tLatches = Abc_NtkRetimePrepareLatches( pNtk );
    // save boxes
    vBoxes = pNtk->vBoxes;  pNtk->vBoxes = NULL;
    // perform the retiming
    if ( fMinDelay )
        Abc_NtkRetimeMinDelay( pNtk, pNtkCopy, nIterLimit, fForward, fVerbose );
    else
        Abc_NtkRetimeOneWay( pNtk, fForward, fVerbose );
    if ( fMinDelay ) 
        Abc_NtkDelete( pNtkCopy );
    // share the latches
    Abc_NtkRetimeShareLatches( pNtk );    
    // restore boxes
    pNtk->vBoxes = vBoxes;
    // finalize the latches
    RetValue = Abc_NtkRetimeFinalizeLatches( pNtk, tLatches, nIdMaxStart );
    st_free_table( tLatches );
    if ( RetValue == 0 )
        return 0;
    // fix the COs
    Abc_NtkLogicMakeSimpleCos( pNtk, 0 );
    // check for correctness
    if ( !Abc_NtkCheck( pNtk ) )
        fprintf( stdout, "Abc_NtkRetimeForward(): Network check has failed.\n" );
    // return the number of latches saved
    return nLatches - Abc_NtkLatchNum(pNtk);
}

/**Function*************************************************************

  Synopsis    [Prepares the network for retiming.]

  Description [Hash latches into their number in the original network.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
st_table * Abc_NtkRetimePrepareLatches( Abc_Ntk_t * pNtk )
{
    st_table * tLatches;
    Abc_Obj_t * pLatch, * pLatchIn, * pLatchOut, * pFanin;
    int i, nOffSet = Abc_NtkBoxNum(pNtk) - Abc_NtkLatchNum(pNtk);
    // collect latches and remove CIs/COs
    tLatches = st_init_table( st_ptrcmp, st_ptrhash );
    Abc_NtkForEachLatch( pNtk, pLatch, i )
    {
        // map latch into its true number
        st_insert( tLatches, (void *)pLatch, (void *)(i-nOffSet) );
        // disconnect LI     
        pLatchIn = Abc_ObjFanin0(pLatch);
        pFanin = Abc_ObjFanin0(pLatchIn);
        Abc_ObjTransferFanout( pLatchIn, pFanin );
        Abc_ObjDeleteFanin( pLatchIn, pFanin );
        // disconnect LO     
        pLatchOut = Abc_ObjFanout0(pLatch);
        pFanin = Abc_ObjFanin0(pLatchOut);
        Abc_ObjTransferFanout( pLatchOut, pFanin );
        Abc_ObjDeleteFanin( pLatchOut, pFanin );
    }
    return tLatches;
}

/**Function*************************************************************

  Synopsis    [Finalizes the latches after retiming.]

  Description [Reuses the LIs/LOs for old latches.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int Abc_NtkRetimeFinalizeLatches( Abc_Ntk_t * pNtk, st_table * tLatches, int nIdMaxStart )
{
    Vec_Ptr_t * vCisOld, * vCosOld, * vBoxesOld, * vCisNew, * vCosNew, * vBoxesNew;
    Abc_Obj_t * pObj, * pLatch, * pLatchIn, * pLatchOut;
    int i, Index;
    // create new arrays
    vCisOld   = pNtk->vCis;    pNtk->vCis   = NULL;  vCisNew   = Vec_PtrAlloc( 100 );
    vCosOld   = pNtk->vCos;    pNtk->vCos   = NULL;  vCosNew   = Vec_PtrAlloc( 100 );  
    vBoxesOld = pNtk->vBoxes;  pNtk->vBoxes = NULL;  vBoxesNew = Vec_PtrAlloc( 100 );
    // copy boxes and their CIs/COs
    Vec_PtrForEachEntryStop( vCisOld, pObj, i, Vec_PtrSize(vCisOld) - st_count(tLatches) )
        Vec_PtrPush( vCisNew, pObj );
    Vec_PtrForEachEntryStop( vCosOld, pObj, i, Vec_PtrSize(vCosOld) - st_count(tLatches) )
        Vec_PtrPush( vCosNew, pObj );
    Vec_PtrForEachEntryStop( vBoxesOld, pObj, i, Vec_PtrSize(vBoxesOld) - st_count(tLatches) )
        Vec_PtrPush( vBoxesNew, pObj );
    // go through the latches
    Abc_NtkForEachObj( pNtk, pLatch, i )
    {
        if ( !Abc_ObjIsLatch(pLatch) )
            continue;
        if ( Abc_ObjId(pLatch) >= (unsigned)nIdMaxStart )
        {
            // this is a new latch 
            pLatchIn  = Abc_NtkCreateBi(pNtk);
            pLatchOut = Abc_NtkCreateBo(pNtk);
            Abc_ObjAssignName( pLatchIn,  Abc_ObjName(pLatchIn), NULL );
            Abc_ObjAssignName( pLatchOut, Abc_ObjName(pLatchOut), NULL );
        }
        else
        {
            // this is an old latch 
            // get its number in the original order
            if ( !st_lookup( tLatches, (char *)pLatch, (char **)&Index ) )
            {
                printf( "Abc_NtkRetimeFinalizeLatches(): Internal error.\n" );
                return 0;
            }
            assert( pLatch == Vec_PtrEntry(vBoxesOld, Vec_PtrSize(vBoxesOld) - st_count(tLatches) + Index) );
            // reconnect with the old LIs/LOs
            pLatchIn  = Vec_PtrEntry( vCosOld, Vec_PtrSize(vCosOld) - st_count(tLatches) + Index );
            pLatchOut = Vec_PtrEntry( vCisOld, Vec_PtrSize(vCisOld) - st_count(tLatches) + Index );
        }
        // connect
        Abc_ObjAddFanin( pLatchIn, Abc_ObjFanin0(pLatch) );
        Abc_ObjPatchFanin( pLatch, Abc_ObjFanin0(pLatch), pLatchIn );
        Abc_ObjTransferFanout( pLatch, pLatchOut );
        Abc_ObjAddFanin( pLatchOut, pLatch );
        // add to the arrays
        Vec_PtrPush( vCisNew, pLatchOut );
        Vec_PtrPush( vCosNew, pLatchIn );
        Vec_PtrPush( vBoxesNew, pLatch );
    }
    // free useless Cis/Cos
    Vec_PtrForEachEntry( vCisOld, pObj, i )
        if ( !Abc_ObjIsPi(pObj) && Abc_ObjFaninNum(pObj) == 0 && Abc_ObjFanoutNum(pObj) == 0 )
            Abc_NtkDeleteObj(pObj);
    Vec_PtrForEachEntry( vCosOld, pObj, i )
        if ( !Abc_ObjIsPo(pObj) && Abc_ObjFaninNum(pObj) == 0 && Abc_ObjFanoutNum(pObj) == 0 )
            Abc_NtkDeleteObj(pObj);
    // set the new arrays
    pNtk->vCis   = vCisNew;   Vec_PtrFree( vCisOld );
    pNtk->vCos   = vCosNew;   Vec_PtrFree( vCosOld );
    pNtk->vBoxes = vBoxesNew; Vec_PtrFree( vBoxesOld );
    return 1;
}

/**Function*************************************************************

  Synopsis    [Performs retiming one way, forward or backward.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int Abc_NtkRetimeOneWay( Abc_Ntk_t * pNtk, int fForward, int fVerbose )
{
    Abc_Ntk_t * pNtkNew;
    Vec_Int_t * vValues;
    Abc_Obj_t * pObj;
    int i, fChanges, nTotalMoves = 0, nTotalMovesLimit = 5000;
    if ( fForward )
        Abc_NtkRetimeTranferToCopy( pNtk );
    else
    {
        // save initial values of the latches
        vValues = Abc_NtkRetimeCollectLatchValues( pNtk );
        // start the network for initial value computation
        pNtkNew = Abc_NtkRetimeBackwardInitialStart( pNtk );
    }
    // try to move latches forward whenever possible
    do {
        fChanges = 0;
        Abc_NtkForEachObj( pNtk, pObj, i )
        {
            if ( !Abc_ObjIsNode(pObj) )
                continue;
            if ( Abc_NtkRetimeNodeIsEnabled( pObj, fForward ) )
            {
                Abc_NtkRetimeNode( pObj, fForward, 1 );
                fChanges = 1;
                nTotalMoves++;
                if ( nTotalMoves >= nTotalMovesLimit )
                {
                    printf( "Stopped after %d latch moves.\n", nTotalMoves );
                    break;
                }
            }
        }
    } while ( fChanges && nTotalMoves < nTotalMovesLimit );
    // transfer the initial state back to the latches
    if ( fForward )
        Abc_NtkRetimeTranferFromCopy( pNtk );
    else
    {
        Abc_NtkRetimeBackwardInitialFinish( pNtk, pNtkNew, vValues, fVerbose );
        Abc_NtkDelete( pNtkNew );
        Vec_IntFree( vValues );
    }
    return 0;
}


/**Function*************************************************************

  Synopsis    [Returns 1 if retiming forward/backward is possible.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int Abc_NtkRetimeNodeIsEnabled( Abc_Obj_t * pObj, int fForward )
{
    Abc_Obj_t * pNext;
    int i;
    assert( Abc_ObjIsNode(pObj) );
    if ( fForward )
    {
        Abc_ObjForEachFanin( pObj, pNext, i )
            if ( !Abc_ObjIsLatch(pNext) )
                return 0;
    }
    else
    {
        Abc_ObjForEachFanout( pObj, pNext, i )
            if ( !Abc_ObjIsLatch(pNext) )
                return 0;
    }
    return 1;
}

/**Function*************************************************************

  Synopsis    [Retimes the node backward or forward.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Abc_NtkRetimeNode( Abc_Obj_t * pObj, int fForward, int fInitial )
{
    Abc_Ntk_t * pNtkNew = NULL;
    Vec_Ptr_t * vNodes;
    Abc_Obj_t * pNext, * pLatch;
    int i;
    vNodes = Vec_PtrAlloc( 10 );
    if ( fForward )
    {
        // compute the initial value
        if ( fInitial )
            pObj->pCopy = (void *)Abc_ObjSopSimulate( pObj );
        // collect fanins
        Abc_NodeCollectFanins( pObj, vNodes );
        // make the node point to the fanins fanins
        Vec_PtrForEachEntry( vNodes, pNext, i )
        {
            assert( Abc_ObjIsLatch(pNext) );
            Abc_ObjPatchFanin( pObj, pNext, Abc_ObjFanin0(pNext) );
            if ( Abc_ObjFanoutNum(pNext) == 0 )
                Abc_NtkDeleteObj(pNext);            
        }
        // add a new latch on top
        pNext = Abc_NtkCreateLatch(pObj->pNtk);
        Abc_ObjTransferFanout( pObj, pNext );
        Abc_ObjAddFanin( pNext, pObj );
        // set the initial value
        if ( fInitial )
            pNext->pCopy = pObj->pCopy;
    }
    else
    {
        // compute the initial value
        if ( fInitial )
        {
            pNtkNew = Abc_ObjFanout0(pObj)->pCopy->pNtk;
            Abc_NtkDupObj( pNtkNew, pObj, 0 );
            Abc_ObjForEachFanout( pObj, pNext, i )
            {
                assert( Abc_ObjFaninNum(pNext->pCopy) == 0 );
                Abc_ObjAddFanin( pNext->pCopy, pObj->pCopy );
            }
        }
        // collect fanouts
        Abc_NodeCollectFanouts( pObj, vNodes );
        // make the fanouts fanouts point to the node
        Vec_PtrForEachEntry( vNodes, pNext, i )
        {
            assert( Abc_ObjIsLatch(pNext) );
            Abc_ObjTransferFanout( pNext, pObj );
            Abc_NtkDeleteObj( pNext );  
        }
        // add new latches to the fanins
        Abc_ObjForEachFanin( pObj, pNext, i )
        {
            pLatch = Abc_NtkCreateLatch(pObj->pNtk);
            Abc_ObjPatchFanin( pObj, pNext, pLatch );
            Abc_ObjAddFanin( pLatch, pNext );
            // create buffer isomorphic to this latch
            if ( fInitial )
            {
                pLatch->pCopy = Abc_NtkCreateNodeBuf( pNtkNew, NULL );
                Abc_ObjAddFanin( pObj->pCopy, pLatch->pCopy );
            }
        }
    }
    Vec_PtrFree( vNodes );
}

/**Function*************************************************************

  Synopsis    [Returns the number of compatible fanout latches.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int Abc_NtkRetimeCheckCompatibleLatchFanouts( Abc_Obj_t * pObj )
{
    Abc_Obj_t * pFanout;
    int i, nLatches = 0, Init = -1;
    Abc_ObjForEachFanout( pObj, pFanout, i )
    {
        if ( !Abc_ObjIsLatch(pFanout) )
            continue;
        if ( Init == -1 )
        {
            Init = (int)pObj->pData;
            nLatches++;
        }
        else if ( Init == (int)pObj->pData )
            nLatches++;
    }
    return nLatches;
}

/**Function*************************************************************

  Synopsis    [Retimes the node backward or forward.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Abc_NtkRetimeShareLatches( Abc_Ntk_t * pNtk )
{
    Vec_Ptr_t * vNodes;
    Abc_Obj_t * pFanin, * pLatch, * pLatchTop, * pLatchCur;
    int i, k;
    vNodes = Vec_PtrAlloc( 10 );
    // consider latch fanins
    Abc_NtkForEachObj( pNtk, pLatch, i )
    {
        if ( !Abc_ObjIsLatch(pLatch) )
            continue;
        pFanin = Abc_ObjFanin0(pLatch);
        if ( Abc_NtkRetimeCheckCompatibleLatchFanouts(pFanin) <= 1 )
            continue;
        // get the first latch
        Abc_ObjForEachFanout( pFanin, pLatchTop, k )
            if ( Abc_ObjIsLatch(pLatchTop) )
                break;
        assert( Abc_ObjIsLatch(pLatchTop) );
        // redirect compatible fanout latches to the first latch
        Abc_NodeCollectFanouts( pFanin, vNodes );
        Vec_PtrForEachEntry( vNodes, pLatchCur, k )
        {
            if ( pLatchCur == pLatchTop )
                continue;
            if ( !Abc_ObjIsLatch(pLatchCur) )
                continue;
            if ( pLatchCur->pData != pLatchTop->pData )
                continue;
            // redirect the fanouts
            Abc_ObjTransferFanout( pLatchCur, pLatchTop );
            Abc_NtkDeleteObj(pLatchCur);
        }
    }
    Vec_PtrFree( vNodes );
}

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////


