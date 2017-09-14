//
// Copyright (C) 2016, 2017 Karl Wette
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with with program; see the file COPYING. If not, write to the
// Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston,
// MA 02111-1307 USA
//

///
/// \file
/// \ingroup lalapps_pulsar_Weave
///

#include "CacheResults.h"

#include <lal/LALHeap.h>
#include <lal/LALHashTbl.h>
#include <lal/LALBitset.h>

// Compare two quantities, and return a sort order value if they are unequal
#define COMPARE_BY( x, y ) do { if ( (x) < (y) ) return -1; if ( (x) > (y) ) return +1; } while(0)

///
/// Internal definition of a cache item stored in the index hash table
///
typedef struct {
  /// Frequency partition index, used to find hash items
  UINT4 partition_index;
  /// Relevance, used to decide how long to keep hash items
  REAL4 relevance;
  /// Coherent locator index, used to find hash items
  UINT8 coh_index;
  /// Results of a coherent computation on a single segment
  WeaveCohResults *coh_res;
} cache_index_hash_item;

///
/// Internal definition of a cache item stored in the relevance heap
///
typedef struct {
  /// Frequency partition index, to decide how long to keep heap items
  UINT4 partition_index;
  /// Relevance, used to decide how long to keep heap items; index hash
  /// table items record their own relevance so it can be updated
  REAL4 relevance;
  /// Coherent locator index, used to find corresponding hash item
  UINT8 coh_index;
} cache_relevance_heap_item;

///
/// Internal definition of cache
///
struct tagWeaveCache {
  /// Number of parameter-space dimensions
  size_t ndim;
  /// Lowest tiled parameter-space dimension
  size_t dim0;
  /// Reduced supersky transform data for coherent lattice
  const SuperskyTransformData *coh_rssky_transf;
  /// Reduced supersky transform data for semicoherent lattice
  const SuperskyTransformData *semi_rssky_transf;
  /// Input data required for computing coherent results
  WeaveCohInput *coh_input;
  /// Coherent parameter-space tiling locator
  LatticeTilingLocator *coh_locator;
  /// Hash table which looks up cache items by index
  LALHashTbl *index_hash;
  /// Bitset which records whether an item has ever been computed
  LALBitset *coh_computed_bitset;
  /// Partition index for which 'coh_computed_bitset' applies
  UINT4 coh_computed_bitset_partition_index;
  /// Heap which ranks cache items by relevance
  LALHeap *relevance_heap;
  /// Offset used in computation of coherent point relevance
  double coh_relevance_offset;
  /// Maximum number of cache items to store
  UINT4 max_size;
  /// Additional cache item garbage collection limit
  UINT4 gc_extra;
  /// Save an no-longer-used index hash item for re-use
  cache_index_hash_item *saved_hash_item;
  /// Save an no-longer-used relevance heap item for re-use
  cache_relevance_heap_item *saved_heap_item;
};

///
/// Internal definition of results from a series of cache queries
///
struct tagWeaveCacheQueries {
  /// Number of parameter-space dimensions
  size_t ndim;
  /// Lowest tiled parameter-space dimension
  size_t dim0;
  /// Number of queries for which space is allocated
  UINT4 nqueries;
  /// Number of partitions to divide semicoherent frequency block into
  UINT4 npartitions;
  /// Index to current partition of semicoherent frequency block
  UINT4 partition_index;
  /// Offset to apply to coherent left-most index to enclose a frequency partition
  INT4 *coh_part_left_offset;
  /// Offset to apply to coherent right-most index to enclose a frequency partition
  INT4 *coh_part_right_offset;
  /// Sequential indexes for each queried coherent frequency block
  UINT8 *coh_index;
  /// Physical points of each queried coherent frequency block
  PulsarDopplerParams *coh_phys;
  /// Indexes of left-most point in queried coherent frequency block
  INT4 *coh_left;
  /// Indexes of right-most point in queried coherent frequency block
  INT4 *coh_right;
  /// Relevance of each queried coherent frequency block
  REAL4 *coh_relevance;
  /// Reduced supersky transform data for semicoherent lattice
  const SuperskyTransformData *semi_rssky_transf;
  /// Sequential index for the current semicoherent frequency block
  UINT8 semi_index;
  /// Current semicoherent reduced supersky point in lowest tiled dimension
  double semi_point_dim0;
  /// Physical coordinates of the current semicoherent frequency block
  PulsarDopplerParams semi_phys;
  /// Index of left-most point in current semicoherent frequency block
  INT4 semi_left;
  /// Index of right-most point in current semicoherent frequency block
  INT4 semi_right;
  /// Relevance of the current semicoherent frequency block
  REAL4 semi_relevance;
  /// Offset used in computation of semicoherent point relevance
  double semi_relevance_offset;
};

///
/// \name Internal functions
///
/// @{

static UINT8 cache_index_hash_item_hash( const void *x );
static int cache_index_hash_item_compare_by_index( const void *x, const void *y );
static int cache_index_hash_item_compare_by_relevance( const void *x, const void *y );
static int cache_left_right_offsets( const UINT4 semi_nfreqs, const UINT4 npartitions, const UINT4 partition_index, INT4 *left_offset, INT4 *right_offset );
static int cache_max_semi_bbox_sample_dim0( const WeaveCache *cache, const LatticeTiling *coh_tiling, const size_t i, gsl_vector *coh_bbox_sample, gsl_vector *semi_bbox_sample, double *max_semi_bbox_sample_dim0 );
static int cache_relevance_heap_item_compare_by_relevance( const void *x, const void *y );
static void cache_index_hash_item_destroy( void *x );
static void cache_relevance_heap_item_destroy( void *x );

/// @}

///
/// Destroy a index hash table item
///
void cache_index_hash_item_destroy(
  void *x
  )
{
  if ( x != NULL ) {
    cache_index_hash_item *ix = ( cache_index_hash_item * ) x;
    XLALWeaveCohResultsDestroy( ix->coh_res );
    XLALFree( ix );
  }
} // cache_index_hash_item_destroy()

///
/// Compare index hash table items by partition index, then locator index
///
int cache_index_hash_item_compare_by_index(
  const void *x,
  const void *y
  )
{
  const cache_index_hash_item *ix = ( const cache_index_hash_item * ) x;
  const cache_index_hash_item *iy = ( const cache_index_hash_item * ) y;
  COMPARE_BY( ix->partition_index, iy->partition_index );   // Compare in ascending order
  COMPARE_BY( ix->coh_index, iy->coh_index );   // Compare in ascending order
  return 0;
} // cache_index_hash_item_compare_by_index()

///
/// Compare index hash table items by partition index, then relevance
///
int cache_index_hash_item_compare_by_relevance(
  const void *x,
  const void *y
  )
{
  const cache_index_hash_item *ix = ( const cache_index_hash_item * ) x;
  const cache_index_hash_item *iy = ( const cache_index_hash_item * ) y;
  COMPARE_BY( ix->partition_index, iy->partition_index );   // Compare in ascending order
  COMPARE_BY( ix->relevance, iy->relevance );   // Compare in ascending order
  return 0;
} // cache_index_hash_item_compare_by_relevance()

///
/// Hash index hash table items by partition index and locator index
///
UINT8 cache_index_hash_item_hash(
  const void *x
  )
{
  const cache_index_hash_item *ix = ( const cache_index_hash_item * ) x;
  UINT4 hval = 0;
  XLALPearsonHash( &hval, sizeof( hval ), &ix->partition_index, sizeof( ix->partition_index ) );
  XLALPearsonHash( &hval, sizeof( hval ), &ix->coh_index, sizeof( ix->coh_index ) );
  return hval;
} // cache_index_hash_item_hash()

///
/// Destroy a relevance heap item
///
void cache_relevance_heap_item_destroy(
  void *x
  )
{
  XLALFree( x );
} // cache_relevance_heap_item_destroy()

///
/// Compare relevance heap items by partition index, then relevance
///
int cache_relevance_heap_item_compare_by_relevance(
  const void *x,
  const void *y
  )
{
  const cache_relevance_heap_item *ix = ( const cache_relevance_heap_item * ) x;
  const cache_relevance_heap_item *iy = ( const cache_relevance_heap_item * ) y;
  COMPARE_BY( ix->partition_index, iy->partition_index );   // Compare in ascending order
  COMPARE_BY( ix->relevance, iy->relevance );   // Compare in ascending order
  return 0;
} // cache_relevance_heap_item_compare_by_relevance()

///
/// Sample points on surface of coherent bounding box, convert to semicoherent supersky
/// coordinates, and record maximum value of semicoherent coordinate in dimension 'dim0'
///
int cache_max_semi_bbox_sample_dim0(
  const WeaveCache *cache,
  const LatticeTiling *coh_tiling,
  const size_t i,
  gsl_vector *coh_bbox_sample,
  gsl_vector *semi_bbox_sample,
  double *max_semi_bbox_sample_dim0
  )
{

  if ( i < cache->ndim ) {

    // Get coherent lattice tiling bounding box in dimension 'i'
    const double coh_bbox_i = XLALLatticeTilingBoundingBox( coh_tiling, i );
    XLAL_CHECK( xlalErrno == 0, XLAL_EFUNC );

    // Get current value of 'coh_bbox_sample' in dimension 'i'
    const double coh_bbox_sample_i = gsl_vector_get( coh_bbox_sample, i );

    // Move 'coh_bbox_sample' in dimension 'i' to vertices, edge centres and face centres of bounding box, and proceed to higher dimensions
    for ( int step = -1; step <= 1; ++ step ) {
      gsl_vector_set( coh_bbox_sample, i, coh_bbox_sample_i - step * 0.5 * coh_bbox_i );
      XLAL_CHECK( cache_max_semi_bbox_sample_dim0( cache, coh_tiling, i + 1, coh_bbox_sample, semi_bbox_sample, max_semi_bbox_sample_dim0 ) == XLAL_SUCCESS, XLAL_EFUNC );
    }

    // Restore current value of 'coh_bbox_sample' in dimension 'i'
    gsl_vector_set( coh_bbox_sample, i, coh_bbox_sample_i );

  } else {

    // Convert 'coh_bbox_sample' to semicoherent reduced supersky coordinates
    XLAL_CHECK( XLALConvertSuperskyToSuperskyPoint( semi_bbox_sample, cache->semi_rssky_transf, coh_bbox_sample, cache->coh_rssky_transf ) == XLAL_SUCCESS, XLAL_EINVAL );

    // Record maximum value of semicoherent coordinate in dimension 'dim0'
    *max_semi_bbox_sample_dim0 = GSL_MAX( *max_semi_bbox_sample_dim0, gsl_vector_get( semi_bbox_sample, cache->dim0 ) );

  }

  return XLAL_SUCCESS;

}

///
/// Compute left/right-most index offsets to select a given partition
///
int cache_left_right_offsets(
  const UINT4 semi_nfreqs,
  const UINT4 npartitions,
  const UINT4 partition_index,
  INT4 *left_offset,
  INT4 *right_offset
  )
{

  // Minimum number of points in the current frequency partition
  const UINT4 min_part_nfreqs = semi_nfreqs / npartitions;

  // Excess number of points which must be added to get 'semi_nfreqs' in total
  INT4 excess_nfreqs = semi_nfreqs - npartitions * min_part_nfreqs;

  // Initialise number of points in this frequency partition
  // - If there are excess points, add an extra point
  INT4 part_nfreqs = min_part_nfreqs;
  if ( excess_nfreqs > 0 ) {
    ++part_nfreqs;
  }

  // Compute offsets to left/right-most indexes to the select each given partition
  // - Add 'part_nfreqs' to '*left_offset' for each increase in partition index
  // - Decrease number of excess points; if zero, subtract extra point from 'part_nfreqs'
  *left_offset = 0;
  for ( size_t i = 0; i < partition_index; ++i ) {
    *left_offset += part_nfreqs;
    --excess_nfreqs;
    if ( excess_nfreqs == 0 ) {
      --part_nfreqs;
    }
  }
  *right_offset = *left_offset + part_nfreqs - semi_nfreqs;
  XLAL_CHECK( *left_offset >= 0, XLAL_EDOM );
  XLAL_CHECK( *right_offset <= 0, XLAL_EDOM );

  return XLAL_SUCCESS;

}

///
/// Create a cache
///
WeaveCache *XLALWeaveCacheCreate(
  const LatticeTiling *coh_tiling,
  const BOOLEAN interpolation,
  const SuperskyTransformData *coh_rssky_transf,
  const SuperskyTransformData *semi_rssky_transf,
  WeaveCohInput *coh_input,
  const UINT4 max_size,
  const UINT4 gc_extra
  )
{

  // Check input
  XLAL_CHECK_NULL( coh_tiling != NULL, XLAL_EFAULT );
  XLAL_CHECK_NULL( coh_input != NULL, XLAL_EFAULT );

  // Allocate memory
  WeaveCache *cache = XLALCalloc( 1, sizeof( *cache ) );
  XLAL_CHECK_NULL( cache != NULL, XLAL_ENOMEM );

  // Set fields
  cache->coh_rssky_transf = coh_rssky_transf;
  cache->semi_rssky_transf = semi_rssky_transf;
  cache->coh_input = coh_input;
  cache->max_size = max_size;
  cache->gc_extra = gc_extra;

  // Get number of parameter-space dimensions
  cache->ndim = XLALTotalLatticeTilingDimensions( coh_tiling );
  XLAL_CHECK_NULL( xlalErrno == 0, XLAL_EFUNC );

  // Get lowest tiled parameter-space dimension
  cache->dim0 = XLALLatticeTilingTiledDimension( coh_tiling, 0 );
  XLAL_CHECK_NULL( xlalErrno == 0, XLAL_EFUNC );

  // Compute offset used in computation of semicoherent point relevance:
  {

    // Convert a physical point far from any parameter-space boundaries to coherent and semicoherent reduced supersky coordinates
    PulsarDopplerParams phys_origin = { .Alpha = 0, .Delta = LAL_PI_2, .fkdot = {0} };
    XLAL_CHECK_NULL( XLALSetPhysicalPointSuperskyRefTime( &phys_origin, cache->coh_rssky_transf ) == XLAL_SUCCESS, XLAL_EFUNC );
    double coh_origin_array[cache->ndim];
    gsl_vector_view coh_origin_view = gsl_vector_view_array( coh_origin_array, cache->ndim );
    XLAL_CHECK_NULL( XLALConvertPhysicalToSuperskyPoint( &coh_origin_view.vector, &phys_origin, cache->coh_rssky_transf ) == XLAL_SUCCESS, XLAL_EFUNC );
    double semi_origin_array[cache->ndim];
    gsl_vector_view semi_origin_view = gsl_vector_view_array( semi_origin_array, cache->ndim );
    XLAL_CHECK_NULL( XLALConvertPhysicalToSuperskyPoint( &semi_origin_view.vector, &phys_origin, cache->semi_rssky_transf ) == XLAL_SUCCESS, XLAL_EFUNC );
    const double semi_origin_dim0 = gsl_vector_get( &semi_origin_view.vector, cache->dim0 );

    // Sample vertices of bounding box around 'coh_origin', and record maximum vertex in semicoherent reduced supersky coordinates in dimension 'dim0'
    double coh_bbox_sample_array[cache->ndim];
    gsl_vector_view coh_bbox_sample_view = gsl_vector_view_array( coh_bbox_sample_array, cache->ndim );
    gsl_vector_memcpy( &coh_bbox_sample_view.vector, &coh_origin_view.vector );
    double semi_bbox_sample_array[cache->ndim];
    gsl_vector_view semi_bbox_sample_view = gsl_vector_view_array( semi_bbox_sample_array, cache->ndim );
    double max_semi_bbox_sample_dim0 = semi_origin_dim0;
    XLAL_CHECK_NULL( cache_max_semi_bbox_sample_dim0( cache, coh_tiling, 0, &coh_bbox_sample_view.vector, &semi_bbox_sample_view.vector, &max_semi_bbox_sample_dim0 ) == XLAL_SUCCESS, XLAL_EFUNC );

    // Subtract off origin of 'semi_origin' to get relevance offset
    cache->coh_relevance_offset = max_semi_bbox_sample_dim0 - semi_origin_dim0;

  }

  // If this is an interpolating search, create a lattice tiling locator
  if ( interpolation ) {
    cache->coh_locator = XLALCreateLatticeTilingLocator( coh_tiling );
    XLAL_CHECK_NULL( cache->coh_locator != NULL, XLAL_EFUNC );
  }

  // Create a hash table which looks up cache items by partition and locator index.
  cache->index_hash = XLALHashTblCreate( cache_index_hash_item_destroy, cache_index_hash_item_hash, cache_index_hash_item_compare_by_index );
  XLAL_CHECK_NULL( cache->index_hash != NULL, XLAL_EFUNC );

  // Create a bitset which records which cache items have previously been computed.
  cache->coh_computed_bitset = XLALBitsetCreate();
  XLAL_CHECK_NULL( cache->coh_computed_bitset != NULL, XLAL_EFUNC );
  cache->coh_computed_bitset_partition_index = LAL_UINT4_MAX;

  // Create a heap which sorts items by "relevance", a quantity which determines how long
  // cache items are kept. Consider the following scenario:
  //
  //   +-----> parameter-space dimension 'dim0'
  //   |
  //   V parameter-space dimensions > 'dim0'
  //
  //        :
  //        : R[S1] = relevance of semicoherent point 'S1'
  //        :
  //        +-----+
  //        | /`\ |
  //        || S1||
  //        | \,/ |
  //        +-----+
  //      +          :
  //     / \         : R[S2] = relevance of semicoherent point 'S2'
  //    /,,,\        :
  //   /(   \\       +-----+
  //  + (    \\      | /`\ |
  //   \\  C  \\     || S2||
  //    \\    ) +    | \,/ |
  //     \\   )/:    +-----+
  //      \```/ :
  //       \ /  :
  //        +   : R[C] = relevance of coherent point 'C'
  //            :
  //
  // The relevance R[C] of the coherent point 'C' is given by the coordinate in dimension 'dim0' of
  // the *rightmost* edge of the bounding box surrounding its metric ellipse. The relevances of
  // two semicoherent points S1 and S2, R[S1] and R[S2], are given by the *leftmost* edges of the
  // bounding box surrounding their metric ellipses.
  //
  // Note that iteration over the parameter space is ordered such that dimension 'dim0' is the slowest
  // (tiled) coordinate, i.e. dimension 'dim0' is passed over only once, therefore coordinates in this
  // dimension are monotonically increasing, therefore relevances are also monotonically increasing.
  //
  // Suppose S1 is the current point in the semicoherent parameter-space tiling; note that R[C] > R[S1].
  // It is clear that, as iteration progresses, some future semicoherent points will overlap with C.
  // Therefore C cannot be discarded from the cache, since it will be the closest point for future
  // semicoherent points. Now suppose that S2 is the current point; note that R[C] < R[S1]. It is clear
  // that neither S2, nor any future point in the semicoherent parameter-space tiling, can ever overlap
  // with C. Therefore C can never be the closest point for any future semicoherent points, and therefore
  // it can safely be discarded from the cache.
  //
  // In short, an item in the cache can be discarded once its relevance falls below the threshold set
  // by the current point semicoherent in the semicoherent parameter-space tiling.
  //
  // Relevances should be updated if a coherent point 'C' is accessed by multiple semicoherent points.
  // Since, however, items in a heap other than the root item cannot be (easily) updated, the relevance
  // heap items contain pointers to corresponding index hash table items, where the relevance can be
  // updated. When an item expires from the relevance heap, the pointed-to index hash table item is
  // examines to see whether it is still relevant, if not then it and the coherent results it points
  // to are destroyed.
  cache->relevance_heap = XLALHeapCreate( cache_relevance_heap_item_destroy, 0, -1, cache_relevance_heap_item_compare_by_relevance );
  XLAL_CHECK_NULL( cache->relevance_heap != NULL, XLAL_EFUNC );

  return cache;

} // XLALWeaveCacheCreate()

///
/// Destroy a cache
///
void XLALWeaveCacheDestroy(
  WeaveCache *cache
  )
{
  if ( cache != NULL ) {
    XLALDestroyLatticeTilingLocator( cache->coh_locator );
    XLALHashTblDestroy( cache->index_hash );
    XLALBitsetDestroy( cache->coh_computed_bitset );
    XLALHeapDestroy( cache->relevance_heap );
    cache_index_hash_item_destroy( cache->saved_hash_item );
    cache_relevance_heap_item_destroy( cache->saved_heap_item );
    XLALFree( cache );
  }
} // XLALWeaveCacheDestroy()

///
/// Create storage for a series of cache queries
///
WeaveCacheQueries *XLALWeaveCacheQueriesCreate(
  const LatticeTiling *semi_tiling,
  const SuperskyTransformData *semi_rssky_transf,
  const UINT4 nqueries,
  const UINT4 npartitions
  )
{

  // Check input
  XLAL_CHECK_NULL( semi_tiling != NULL, XLAL_EFAULT );
  XLAL_CHECK_NULL( nqueries > 0, XLAL_EINVAL );
  XLAL_CHECK_NULL( npartitions > 0, XLAL_EINVAL );

  // Allocate memory
  WeaveCacheQueries *queries = XLALCalloc( 1, sizeof( *queries ) );
  XLAL_CHECK_NULL( queries != NULL, XLAL_ENOMEM );
  queries->coh_part_left_offset = XLALCalloc( npartitions, sizeof( *queries->coh_part_left_offset ) );
  XLAL_CHECK_NULL( queries->coh_part_left_offset != NULL, XLAL_ENOMEM );
  queries->coh_part_right_offset = XLALCalloc( npartitions, sizeof( *queries->coh_part_right_offset ) );
  XLAL_CHECK_NULL( queries->coh_part_right_offset != NULL, XLAL_ENOMEM );
  queries->coh_index = XLALCalloc( nqueries, sizeof( *queries->coh_index ) );
  XLAL_CHECK_NULL( queries->coh_index != NULL, XLAL_ENOMEM );
  queries->coh_phys = XLALCalloc( nqueries, sizeof( *queries->coh_phys ) );
  XLAL_CHECK_NULL( queries->coh_phys != NULL, XLAL_ENOMEM );
  queries->coh_left = XLALCalloc( nqueries, sizeof( *queries->coh_left ) );
  XLAL_CHECK_NULL( queries->coh_left != NULL, XLAL_ENOMEM );
  queries->coh_right = XLALCalloc( nqueries, sizeof( *queries->coh_right ) );
  XLAL_CHECK_NULL( queries->coh_right != NULL, XLAL_ENOMEM );
  queries->coh_relevance = XLALCalloc( nqueries, sizeof( *queries->coh_relevance ) );
  XLAL_CHECK_NULL( queries->coh_relevance != NULL, XLAL_ENOMEM );

  // Set fields
  queries->semi_rssky_transf = semi_rssky_transf;
  queries->nqueries = nqueries;
  queries->npartitions = npartitions;

  // Get number of parameter-space dimensions
  queries->ndim = XLALTotalLatticeTilingDimensions( semi_tiling );
  XLAL_CHECK_NULL( xlalErrno == 0, XLAL_EFUNC );

  // Get lowest tiled parameter-space dimension
  queries->dim0 = XLALLatticeTilingTiledDimension( semi_tiling, 0 );
  XLAL_CHECK_NULL( xlalErrno == 0, XLAL_EFUNC );

  // Compute offset used in computation of semicoherent point relevance:
  // - Negative half of semicoherent lattice tiling bounding box in dimension 'dim0'
  queries->semi_relevance_offset = -0.5 * XLALLatticeTilingBoundingBox( semi_tiling, queries->dim0 );
  XLAL_CHECK_NULL( xlalErrno == 0, XLAL_EFUNC );

  // Minimum number of points in a semicoherent frequency block
  const LatticeTilingStats *stats = XLALLatticeTilingStatistics( semi_tiling, queries->ndim - 1 );
  XLAL_CHECK_NULL( stats != NULL, XLAL_EFUNC );
  XLAL_CHECK_NULL( stats->min_points > 0, XLAL_EFAILED );

  // Compute offsets to coherent left/right-most indexes to enclose each frequency partition
  // - Use 'semi_min_freqs' to make offsets less than or equal to any semicoherent offsets
  for ( size_t i = 0; i < queries->npartitions; ++i ) {
    XLAL_CHECK_NULL( cache_left_right_offsets( stats->min_points, queries->npartitions, i, &queries->coh_part_left_offset[i], &queries->coh_part_right_offset[i] ) == XLAL_SUCCESS, XLAL_EFUNC );
  }

  return queries;

} // XLALWeaveCacheQueriesCreate()

///
/// Destroy storage for a series of cache queries
///
void XLALWeaveCacheQueriesDestroy(
  WeaveCacheQueries *queries
  )
{
  if ( queries != NULL ) {
    XLALFree( queries->coh_part_left_offset );
    XLALFree( queries->coh_part_right_offset );
    XLALFree( queries->coh_index );
    XLALFree( queries->coh_phys );
    XLALFree( queries->coh_left );
    XLALFree( queries->coh_right );
    XLALFree( queries->coh_relevance );
    XLALFree( queries );
  }
} // XLALWeaveCacheQueriesDestroy()

///
/// Initialise a series of cache queries
///
int XLALWeaveCacheQueriesInit(
  WeaveCacheQueries *queries,
  const LatticeTilingIterator *semi_itr,
  const UINT8 semi_index,
  const gsl_vector *semi_point
  )
{

  // Check input
  XLAL_CHECK( queries != NULL, XLAL_EFAULT );
  XLAL_CHECK( semi_itr != NULL, XLAL_EFAULT );

  // Reset coherent sequential indexes to zero, indicating no query
  memset( queries->coh_index, 0, queries->nqueries * sizeof( queries->coh_index[0] ) );

  // Save current semicoherent sequential index
  queries->semi_index = semi_index;

  // Save current semicoherent reduced supersky point in dimension 'dim0'
  queries->semi_point_dim0 = gsl_vector_get( semi_point, queries->dim0 );

  // Convert semicoherent point to physical coordinates
  XLAL_CHECK( XLALConvertSuperskyToPhysicalPoint( &queries->semi_phys, semi_point, NULL, queries->semi_rssky_transf ) == XLAL_SUCCESS, XLAL_EFUNC );

  // Get indexes of left/right-most point in current semicoherent frequency block
  XLAL_CHECK( XLALCurrentLatticeTilingBlock( semi_itr, queries->ndim - 1, &queries->semi_left, &queries->semi_right ) == XLAL_SUCCESS, XLAL_EFUNC );

  // Compute the relevance of the current semicoherent frequency block
  // - Relevance is current semicoherent reduced supersky point in dimension 'dim0', plus offset
  queries->semi_relevance = queries->semi_point_dim0 + queries->semi_relevance_offset;

  return XLAL_SUCCESS;

} // XLALWeaveCacheQueriesInit()

///
/// Query a cache for the results nearest to a given coherent point
///
int XLALWeaveCacheQuery(
  const WeaveCache *cache,
  WeaveCacheQueries *queries,
  const UINT4 query_index
  )
{

  // Check input
  XLAL_CHECK( cache != NULL, XLAL_EFAULT );
  XLAL_CHECK( queries != NULL, XLAL_EFAULT );
  XLAL_CHECK( cache->ndim == queries->ndim, XLAL_ESIZE );
  XLAL_CHECK( cache->dim0 == queries->dim0, XLAL_ESIZE );
  XLAL_CHECK( query_index < queries->nqueries, XLAL_EINVAL );

  // Convert semicoherent physical point to coherent reduced supersky coordinates
  double coh_point_array[cache->ndim];
  gsl_vector_view coh_point_view = gsl_vector_view_array( coh_point_array, cache->ndim );
  XLAL_CHECK( XLALConvertPhysicalToSuperskyPoint( &coh_point_view.vector, &queries->semi_phys, cache->coh_rssky_transf ) == XLAL_SUCCESS, XLAL_EFUNC );

  // Initialise nearest point to semicoherent point in coherent reduced supersky coordinates
  double coh_near_point_array[cache->ndim];
  gsl_vector_view coh_near_point_view = gsl_vector_view_array( coh_near_point_array, cache->ndim );
  gsl_vector_memcpy( &coh_near_point_view.vector, &coh_point_view.vector );

  // Initialise values for a non-interpolating search
  queries->coh_index[query_index] = queries->semi_index;
  queries->coh_left[query_index] = queries->semi_left;
  queries->coh_right[query_index] = queries->semi_right;

  // If performing an interpolating search, find the nearest coherent frequency pass in this segment
  // - 'coh_near_point' is set to the nearest point to the mid-point of the semicoherent frequency block
  // - 'coh_index' is set to the index of this coherent frequency block, used for cache lookup
  // - 'coh_left' and 'coh_right' are set of number of points to the left/right of 'coh_near_point';
  //   check that these are sufficient to contain the number of points to the left/right of 'semi_point'
  if ( cache->coh_locator != NULL ) {
    XLAL_CHECK( XLALNearestLatticeTilingBlock( cache->coh_locator, &coh_point_view.vector, cache->ndim - 1, &coh_near_point_view.vector, &queries->coh_index[query_index], &queries->coh_left[query_index], &queries->coh_right[query_index] ) == XLAL_SUCCESS, XLAL_EFUNC );
    XLAL_CHECK( queries->coh_left[query_index] <= queries->coh_right[query_index], XLAL_EINVAL );
    XLAL_CHECK( queries->coh_left[query_index] <= queries->semi_left && queries->semi_right <= queries->coh_right[query_index], XLAL_EFAILED, "Range of coherent tiling [%i,%i] does not contain semicoherent tiling [%i,%i] at semicoherent index %"LAL_UINT8_FORMAT", query index %u", queries->coh_left[query_index], queries->coh_right[query_index], queries->semi_left, queries->semi_right, queries->semi_index, query_index );
  }

  // Make 'coh_index' a 1-based index, so zero can be used to indicate a missing query
  ++queries->coh_index[query_index];

  // Convert nearest coherent point to physical coordinates
  XLAL_INIT_MEM( queries->coh_phys[query_index] );
  XLAL_CHECK( XLALConvertSuperskyToPhysicalPoint( &queries->coh_phys[query_index], &coh_near_point_view.vector, &coh_point_view.vector, cache->coh_rssky_transf ) == XLAL_SUCCESS, XLAL_EFUNC );

  // Compute the relevance of the current coherent frequency block:
  // - Convert nearest coherent point to semicoherent reduced supersky coordinates
  // - Relevance is maximum of: current semicoherent reduced supersky point in dimension 'dim0', or nearest
  // coherent point in semicoherent reduced supersky coordinates in dimension 'dim0'; plus offset
  {
    double semi_near_point_array[cache->ndim];
    gsl_vector_view semi_near_point_view = gsl_vector_view_array( semi_near_point_array, cache->ndim );
    XLAL_CHECK( XLALConvertSuperskyToSuperskyPoint( &semi_near_point_view.vector, cache->semi_rssky_transf, &coh_near_point_view.vector, cache->coh_rssky_transf ) == XLAL_SUCCESS, XLAL_EINVAL );
    const double max_semi_point_dim0 = GSL_MAX( queries->semi_point_dim0, gsl_vector_get( &semi_near_point_view.vector, cache->dim0 ) );
    queries->coh_relevance[query_index] = max_semi_point_dim0 + cache->coh_relevance_offset;
  }

  return XLAL_SUCCESS;

} // XLALWeaveCacheQuery()

///
/// Finalise a series of cache queries
///
int XLALWeaveCacheQueriesFinal(
  WeaveCacheQueries *queries,
  const UINT4 partition_index,
  PulsarDopplerParams *semi_phys,
  const double dfreq,
  UINT4 *semi_nfreqs
  )
{

  // Check input
  XLAL_CHECK( queries != NULL, XLAL_EFAULT );
  XLAL_CHECK( partition_index < queries->npartitions, XLAL_EINVAL );
  XLAL_CHECK( semi_phys != NULL, XLAL_EFAULT );
  XLAL_CHECK( dfreq >= 0, XLAL_EINVAL );
  XLAL_CHECK( semi_nfreqs != NULL, XLAL_EFAULT );

  // Save index to current partition of semicoherent frequency block
  queries->partition_index = partition_index;

  // Check that every 'coh_index' is at least 1, i.e. a query was made
  for ( size_t i = 0; i < queries->nqueries; ++i ) {
    XLAL_CHECK( queries->coh_index[i] > 0, XLAL_EINVAL, "Missing query at index %zu", i );
  }

  // Compute the total number of points in the semicoherent frequency block
  *semi_nfreqs = queries->semi_right - queries->semi_left + 1;

  // Compute offsets to semicoherent left/right-most indexes to select each frequency partition
  INT4 semi_part_left_offset = 0, semi_part_right_offset = 0;
  XLAL_CHECK( cache_left_right_offsets( *semi_nfreqs, queries->npartitions, queries->partition_index, &semi_part_left_offset, &semi_part_right_offset ) == XLAL_SUCCESS, XLAL_EFUNC );
  XLAL_CHECK( queries->coh_part_left_offset[queries->partition_index] <= semi_part_left_offset && semi_part_right_offset <= queries->coh_part_right_offset[queries->partition_index], XLAL_EFAILED );

  // Adjust semicoherent left/right-most indexes to select the given partition
  // - If there are fewer points in the semicoherent frequency block than partitions,
  //   then some partitions will have 'semi_right' < 'semi_left', i.e. no points. In
  //   which set '*semi_nfreqs' to zero indicates that this partition should be skipped
  //   for this semicoherent frequency block
  queries->semi_left += semi_part_left_offset;
  queries->semi_right += semi_part_right_offset;
  if ( queries->semi_right < queries->semi_left ) {
    *semi_nfreqs = 0;
    return XLAL_SUCCESS;
  }

  // Compute the total number of points in the semicoherent frequency block partition
  *semi_nfreqs = queries->semi_right - queries->semi_left + 1;

  // Adjust coherent left/right-most indexes to enclose the given partition
  for ( size_t i = 0; i < queries->nqueries; ++i ) {
    queries->coh_left[i] += queries->coh_part_left_offset[queries->partition_index];
    queries->coh_right[i] += queries->coh_part_right_offset[queries->partition_index];
  }

  // Shift physical frequencies to first point in coherent/semicoherent frequency block partition
  queries->semi_phys.fkdot[0] += dfreq * queries->semi_left;
  for ( size_t i = 0; i < queries->nqueries; ++i ) {
    queries->coh_phys[i].fkdot[0] += dfreq * queries->coh_left[i];
  }

  // Return first point in semicoherent frequency block partition
  *semi_phys = queries->semi_phys;

  return XLAL_SUCCESS;

} // XLALWeaveCacheQueriesFinal()

///
/// Retrieve coherent results for a given query, or compute new coherent results if not found
///
int XLALWeaveCacheRetrieve(
  WeaveCache *cache,
  WeaveCacheQueries *queries,
  const UINT4 query_index,
  const WeaveCohResults **coh_res,
  UINT8 *coh_index,
  UINT4 *coh_offset,
  UINT8 *coh_nres,
  UINT8 *coh_ntmpl
  )
{

  // Check input
  XLAL_CHECK( cache != NULL, XLAL_EFAULT );
  XLAL_CHECK( queries != NULL, XLAL_EFAULT );
  XLAL_CHECK( queries != NULL, XLAL_EFAULT );
  XLAL_CHECK( query_index < queries->nqueries, XLAL_EINVAL );
  XLAL_CHECK( coh_res != NULL, XLAL_EFAULT );
  XLAL_CHECK( coh_offset != NULL, XLAL_EFAULT );
  XLAL_CHECK( coh_nres != NULL, XLAL_EFAULT );
  XLAL_CHECK( coh_ntmpl != NULL, XLAL_EFAULT );

  // Determine the number of points in the coherent frequency block
  const UINT4 coh_nfreqs = queries->coh_right[query_index] - queries->coh_left[query_index] + 1;

  // Reset bitset if partition index has changed
  if ( cache->coh_computed_bitset_partition_index != queries->partition_index ) {
    XLAL_CHECK( XLALBitsetClear( cache->coh_computed_bitset ) == XLAL_SUCCESS, XLAL_EFUNC );
    cache->coh_computed_bitset_partition_index = queries->partition_index;
  }

  // Try to extract index hash item containing required coherent results
  const cache_index_hash_item find_hash_key = { .partition_index = queries->partition_index, .coh_index = queries->coh_index[query_index] };
  cache_index_hash_item *find_hash_item = NULL;
  XLAL_CHECK( XLALHashTblExtract( cache->index_hash, &find_hash_key, ( void ** ) &find_hash_item ) == XLAL_SUCCESS, XLAL_EFUNC );
  const BOOLEAN find_hash_item_found = ( find_hash_item != NULL );
  if ( !find_hash_item_found ) {

    // Reuse 'saved_hash_item' if possible, otherwise allocate memory for a new index hash item
    if ( cache->saved_hash_item == NULL ) {
      cache->saved_hash_item = XLALCalloc( 1, sizeof( *cache->saved_hash_item ) );
      XLAL_CHECK( cache->saved_hash_item != NULL, XLAL_EINVAL );
    }
    find_hash_item = cache->saved_hash_item;
    cache->saved_hash_item = NULL;

    // Set the key of the new index hash item
    find_hash_item->partition_index = find_hash_key.partition_index;
    find_hash_item->coh_index = find_hash_key.coh_index;

    // Set the relevance of the new index hash item to that of the coherent frequency block
    find_hash_item->relevance = queries->coh_relevance[query_index];

    // Compute coherent results for the new index hash item
    XLAL_CHECK( XLALWeaveCohResultsCompute( &find_hash_item->coh_res, cache->coh_input, &queries->coh_phys[query_index], coh_nfreqs ) == XLAL_SUCCESS, XLAL_EFUNC );

    // Increment number of computed coherent results
    *coh_nres += coh_nfreqs;

    // Check if coherent results have been computed previously
    BOOLEAN computed = 0;
    XLAL_CHECK( XLALBitsetGet( cache->coh_computed_bitset, find_hash_key.coh_index, &computed ) == XLAL_SUCCESS, XLAL_EFUNC );
    if ( !computed ) {

      // Coherent results have not been computed before: increment the number of coherent templates
      *coh_ntmpl += coh_nfreqs;

      // This coherent result has now been computed
      XLAL_CHECK( XLALBitsetSet( cache->coh_computed_bitset, find_hash_key.coh_index, 1 ) == XLAL_SUCCESS, XLAL_EFUNC );

    }

  }

  // Update the relevance of the found index hash item to that of the coherent frequency block
  find_hash_item->relevance = GSL_MAX( find_hash_item->relevance, queries->coh_relevance[query_index] );

  // Remove no longer relevant items from the index hash tree
  UINT4 gc = 0;
  while ( 1 ) {

    // If the (fixed-size) cache is full, then we must remove a cache item in this iteration
    const BOOLEAN cache_is_full = ( 0 < cache->max_size ) && ( cache->max_size == (UINT4) XLALHashTblSize( cache->index_hash ) );

    // Get the root item of the relevance heap, which contains the current least relevant item
    const cache_relevance_heap_item *heap_root = XLALHeapRoot( cache->relevance_heap );
    XLAL_CHECK( xlalErrno == 0, XLAL_EFUNC );
    if ( heap_root == NULL ) {

      // Relevance heap is empty
      break;

    }

    // Check whether relevance heap root item is still relevant, based in current semicoherent point
    // - Bypassed if cache is full
    const cache_relevance_heap_item heap_relevance = { .partition_index = queries->partition_index, .relevance = queries->semi_relevance };
    if ( !cache_is_full && cache_relevance_heap_item_compare_by_relevance( heap_root, &heap_relevance ) >= 0 ) {

      // Cache is not full, and root of relevance heap is still relevant
      break;

    }

    // Check whether index hash item pointed to by relevance heap root item still exists
    const cache_index_hash_item heap_root_hash_key = { .partition_index = queries->partition_index, .coh_index = heap_root->coh_index };
    const cache_index_hash_item* heap_root_hash_item = NULL;
    XLAL_CHECK( XLALHashTblFind( cache->index_hash, &heap_root_hash_key, ( const void ** ) &heap_root_hash_item ) == XLAL_SUCCESS, XLAL_EFUNC );
    if ( heap_root_hash_item != NULL ) {

      // Check whether index hash item pointed to by relevance heap root item is still relevant
      // - Overridden if cache is full
      const cache_index_hash_item hash_relevance = { .partition_index = queries->partition_index, .relevance = queries->semi_relevance };
      if ( cache_is_full || cache_index_hash_item_compare_by_relevance( heap_root_hash_item, &hash_relevance ) < 0 ) {

        // Return if garbage collection limit has been reached
        // - Bypassed if cache is full
        if ( !cache_is_full && gc > cache->gc_extra ) {
          break;
        }

        // Index hash item is no longer relevant, so should be removed
        // - Save hash item to 'saved_hash_item' if possible, otherwise destroy
        if ( cache->saved_hash_item == NULL ) {
          XLAL_CHECK( XLALHashTblExtract( cache->index_hash, heap_root_hash_item, ( void ** ) &cache->saved_hash_item ) == XLAL_SUCCESS, XLAL_EFUNC );
        } else {
          XLAL_CHECK( XLALHashTblRemove( cache->index_hash, heap_root_hash_item ) == XLAL_SUCCESS, XLAL_EFUNC );
        }

        // Increment garbage collection count
        // - Bypassed if cache is full
        if ( !cache_is_full ) {
          ++gc;
        }

      }

    }

    // Root of relevance heap is no longer relevant, so should be removed
    // - Save hash item to 'saved_heap_item' if possible, otherwise destroy
    if ( cache->saved_heap_item == NULL ) {
      cache->saved_heap_item = XLALHeapExtractRoot( cache->relevance_heap );
      XLAL_CHECK( cache->saved_heap_item != NULL, XLAL_EFUNC );
    } else {
      XLAL_CHECK( XLALHeapRemoveRoot( cache->relevance_heap ) == XLAL_SUCCESS, XLAL_EFUNC );
    }

  }

  // Reinsert index hash item containing required coherent results
  XLAL_CHECK( XLALHashTblAdd( cache->index_hash, find_hash_item ) == XLAL_SUCCESS, XLAL_EFUNC );

  // Add a new relevance heap item to track index hash item at its current relevance
  {

    // Reuse 'saved_heap_item' if possible, otherwise allocate memory for a new index heap item
    if ( cache->saved_heap_item == NULL ) {
      cache->saved_heap_item = XLALCalloc( 1, sizeof( *cache->saved_heap_item ) );
      XLAL_CHECK( cache->saved_heap_item != NULL, XLAL_EINVAL );
    }
    cache_relevance_heap_item *find_heap_item = cache->saved_heap_item;
    cache->saved_heap_item = NULL;

    // Set the key of the new relevance heap item
    find_heap_item->partition_index = find_hash_item->partition_index;
    find_heap_item->relevance = find_hash_item->relevance;

    // Set the locator index of the heap item to point to the hash item
    find_heap_item->coh_index = find_hash_item->coh_index;

    // Insert the item into the relevance heap
    // - 'find_heap_item' should now be NULL since relevance heap has no limit
    XLAL_CHECK( XLALHeapAdd( cache->relevance_heap, ( void ** ) &find_heap_item ) == XLAL_SUCCESS, XLAL_EFUNC );
    XLAL_CHECK( find_heap_item == NULL, XLAL_EFAILED );

  }

  // Return coherent results from cache
  *coh_res = find_hash_item->coh_res;

  // Return index of coherent result
  *coh_index = find_hash_item->coh_index;

  // Return offset at which coherent results should be combined with semicoherent results
  *coh_offset = queries->semi_left - queries->coh_left[query_index];

  return XLAL_SUCCESS;

} // XLALWeaveCacheRetrieve()

// Local Variables:
// c-file-style: "linux"
// c-basic-offset: 2
// End:
