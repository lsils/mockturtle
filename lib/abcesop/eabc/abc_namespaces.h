/**CFile****************************************************************

  FileName    [abc_namespaces.h]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Namespace logic.]

  Synopsis    []

  Author      [Alan Mishchenko]

  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - Nov 20, 2015.]

  Revision    []

***********************************************************************/

#ifndef ABC__misc__util__abc_namespaces_h
#define ABC__misc__util__abc_namespaces_h


////////////////////////////////////////////////////////////////////////
///                         NAMESPACES                               ///
////////////////////////////////////////////////////////////////////////

#define ABC_NAMESPACE_HEADER_START namespace eabc {
#define ABC_NAMESPACE_HEADER_END }
#define ABC_NAMESPACE_CXX_HEADER_START ABC_NAMESPACE_HEADER_START
#define ABC_NAMESPACE_CXX_HEADER_END ABC_NAMESPACE_HEADER_END
#define ABC_NAMESPACE_IMPL_START namespace eabc {
#define ABC_NAMESPACE_IMPL_END }
#define ABC_NAMESPACE_PREFIX eabc::
#define ABC_NAMESPACE_USING_NAMESPACE using namespace eabc;

#endif // #ifndef ABC__misc__util__abc_namespaces_h

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////
