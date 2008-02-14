/** @file

Copyright (c) 2005 - 2006, Intel Corporation
All rights reserved. This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.


Module Name:

  Ip4Route.c

Abstract:


**/

#include "Ip4Impl.h"


/**
  Allocate a route entry then initialize it with the Dest/Netmaks
  and Gateway.

  @param  Dest                  The destination network
  @param  Netmask               The destination network mask
  @param  GateWay               The nexthop address

  @return NULL if failed to allocate memeory, otherwise the newly created
  @return route entry.

**/
STATIC
IP4_ROUTE_ENTRY *
Ip4CreateRouteEntry (
  IN IP4_ADDR               Dest,
  IN IP4_ADDR               Netmask,
  IN IP4_ADDR               GateWay
  )
{
  IP4_ROUTE_ENTRY           *RtEntry;

  RtEntry = AllocatePool (sizeof (IP4_ROUTE_ENTRY));

  if (RtEntry == NULL) {
    return NULL;
  }

  InitializeListHead (&RtEntry->Link);

  RtEntry->RefCnt  = 1;
  RtEntry->Dest    = Dest;
  RtEntry->Netmask = Netmask;
  RtEntry->NextHop = GateWay;
  RtEntry->Flag    = 0;

  return RtEntry;
}


/**
  Free the route table entry. It is reference counted.

  @param  RtEntry               The route entry to free.

  @return NONE

**/
STATIC
VOID
Ip4FreeRouteEntry (
  IN IP4_ROUTE_ENTRY    *RtEntry
  )
{
  ASSERT (RtEntry->RefCnt > 0);

  if (--RtEntry->RefCnt == 0) {
    gBS->FreePool (RtEntry);
  }
}


/**
  Allocate and initialize a IP4 route cache entry.

  @param  Dst                   The destination address
  @param  Src                   The source address
  @param  GateWay               The next hop address
  @param  Tag                   The tag from the caller. This marks all the cache
                                entries spawned from one route table entry.

  @return NULL if failed to allocate memory for the cache, other point
  @return to the created route cache entry.

**/
STATIC
IP4_ROUTE_CACHE_ENTRY *
Ip4CreateRouteCacheEntry (
  IN IP4_ADDR               Dst,
  IN IP4_ADDR               Src,
  IN IP4_ADDR               GateWay,
  IN UINTN                  Tag
  )
{
  IP4_ROUTE_CACHE_ENTRY     *RtCacheEntry;

  RtCacheEntry = AllocatePool (sizeof (IP4_ROUTE_CACHE_ENTRY));

  if (RtCacheEntry == NULL) {
    return NULL;
  }

  InitializeListHead (&RtCacheEntry->Link);

  RtCacheEntry->RefCnt  = 1;
  RtCacheEntry->Dest    = Dst;
  RtCacheEntry->Src     = Src;
  RtCacheEntry->NextHop = GateWay;
  RtCacheEntry->Tag     = Tag;

  return RtCacheEntry;
}


/**
  Free the route cache entry. It is reference counted.

  @param  RtCacheEntry          The route cache entry to free.

  @return None

**/
VOID
Ip4FreeRouteCacheEntry (
  IN IP4_ROUTE_CACHE_ENTRY  *RtCacheEntry
  )
{
  ASSERT (RtCacheEntry->RefCnt > 0);

  if (--RtCacheEntry->RefCnt == 0) {
    gBS->FreePool (RtCacheEntry);
  }
}


/**
  Initialize an empty route cache table.

  @param  RtCache               The rotue cache table to initialize.

  @return NONE

**/
VOID
Ip4InitRouteCache (
  IN IP4_ROUTE_CACHE        *RtCache
  )
{
  UINT32                    Index;

  for (Index = 0; Index < IP4_ROUTE_CACHE_HASH; Index++) {
    InitializeListHead (&(RtCache->CacheBucket[Index]));
  }
}


/**
  Clean up a route cache, that is free all the route cache
  entries enqueued in the cache.

  @param  RtCache               The route cache table to clean up

  @return None

**/
VOID
Ip4CleanRouteCache (
  IN IP4_ROUTE_CACHE        *RtCache
  )
{
  LIST_ENTRY                *Entry;
  LIST_ENTRY                *Next;
  IP4_ROUTE_CACHE_ENTRY     *RtCacheEntry;
  UINT32                    Index;

  for (Index = 0; Index < IP4_ROUTE_CACHE_HASH; Index++) {
    NET_LIST_FOR_EACH_SAFE (Entry, Next, &(RtCache->CacheBucket[Index])) {
      RtCacheEntry = NET_LIST_USER_STRUCT (Entry, IP4_ROUTE_CACHE_ENTRY, Link);

      RemoveEntryList (Entry);
      Ip4FreeRouteCacheEntry (RtCacheEntry);
    }
  }
}



/**
  Create an empty route table, includes its internal route cache

  None

  @return NULL if failed to allocate memory for the route table, otherwise
  @return the point to newly created route table.

**/
IP4_ROUTE_TABLE *
Ip4CreateRouteTable (
  VOID
  )
{
  IP4_ROUTE_TABLE           *RtTable;
  UINT32                    Index;

  RtTable = AllocatePool (sizeof (IP4_ROUTE_TABLE));

  if (RtTable == NULL) {
    return NULL;
  }

  RtTable->RefCnt   = 1;
  RtTable->TotalNum = 0;

  for (Index = 0; Index < IP4_MASK_NUM; Index++) {
    InitializeListHead (&(RtTable->RouteArea[Index]));
  }

  RtTable->Next = NULL;

  Ip4InitRouteCache (&RtTable->Cache);
  return RtTable;
}


/**
  Free the route table and its associated route cache. Route
  table is reference counted.

  @param  RtTable               The route table to free.

  @return None

**/
VOID
Ip4FreeRouteTable (
  IN IP4_ROUTE_TABLE        *RtTable
  )
{
  LIST_ENTRY                *Entry;
  LIST_ENTRY                *Next;
  IP4_ROUTE_ENTRY           *RtEntry;
  UINT32                    Index;

  ASSERT (RtTable->RefCnt > 0);

  if (--RtTable->RefCnt > 0) {
    return ;
  }

  //
  // Free all the route table entry and its route cache.
  //
  for (Index = 0; Index < IP4_MASK_NUM; Index++) {
    NET_LIST_FOR_EACH_SAFE (Entry, Next, &(RtTable->RouteArea[Index])) {
      RtEntry = NET_LIST_USER_STRUCT (Entry, IP4_ROUTE_ENTRY, Link);

      RemoveEntryList (Entry);
      Ip4FreeRouteEntry (RtEntry);
    }
  }

  Ip4CleanRouteCache (&RtTable->Cache);

  gBS->FreePool (RtTable);
}



/**
  Remove all the cache entries bearing the Tag. When a route cache
  entry is created, it is tagged with the address of route entry
  from which it is spawned. When a route entry is deleted, the cache
  entries spawned from it are also deleted.

  @param  RtCache               Route cache to remove the entries from
  @param  Tag                   The Tag of the entries to remove

  @return None

**/
STATIC
VOID
Ip4PurgeRouteCache (
  IN IP4_ROUTE_CACHE        *RtCache,
  IN UINTN                  Tag
  )
{
  LIST_ENTRY                *Entry;
  LIST_ENTRY                *Next;
  IP4_ROUTE_CACHE_ENTRY     *RtCacheEntry;
  UINT32                    Index;

  for (Index = 0; Index < IP4_ROUTE_CACHE_HASH; Index++) {
    NET_LIST_FOR_EACH_SAFE (Entry, Next, &RtCache->CacheBucket[Index]) {

      RtCacheEntry = NET_LIST_USER_STRUCT (Entry, IP4_ROUTE_CACHE_ENTRY, Link);

      if (RtCacheEntry->Tag == Tag) {
        RemoveEntryList (Entry);
        Ip4FreeRouteCacheEntry (RtCacheEntry);
      }
    }
  }
}


/**
  Add a route entry to the route table. All the IP4_ADDRs are in
  host byte order.

  @param  RtTable               Route table to add route to
  @param  Dest                  The destination of the network
  @param  Netmask               The netmask of the destination
  @param  Gateway               The next hop address

  @retval EFI_ACCESS_DENIED     The same route already exists
  @retval EFI_OUT_OF_RESOURCES  Failed to allocate memory for the entry
  @retval EFI_SUCCESS           The route is added successfully.

**/
EFI_STATUS
Ip4AddRoute (
  IN IP4_ROUTE_TABLE        *RtTable,
  IN IP4_ADDR               Dest,
  IN IP4_ADDR               Netmask,
  IN IP4_ADDR               Gateway
  )
{
  LIST_ENTRY                *Head;
  LIST_ENTRY                *Entry;
  IP4_ROUTE_ENTRY           *RtEntry;

  //
  // All the route entries with the same netmask length are
  // linke to the same route area
  //
  Head = &(RtTable->RouteArea[NetGetMaskLength (Netmask)]);

  //
  // First check whether the route exists
  //
  NET_LIST_FOR_EACH (Entry, Head) {
    RtEntry = NET_LIST_USER_STRUCT (Entry, IP4_ROUTE_ENTRY, Link);

    if (IP4_NET_EQUAL (RtEntry->Dest, Dest, Netmask) && (RtEntry->NextHop == Gateway)) {
      return EFI_ACCESS_DENIED;
    }
  }

  //
  // Create a route entry and insert it to the route area.
  //
  RtEntry = Ip4CreateRouteEntry (Dest, Netmask, Gateway);

  if (RtEntry == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  if (Gateway == IP4_ALLZERO_ADDRESS) {
    RtEntry->Flag = IP4_DIRECT_ROUTE;
  }

  InsertHeadList (Head, &RtEntry->Link);
  RtTable->TotalNum++;

  return EFI_SUCCESS;
}


/**
  Remove a route entry and all the route caches spawn from it.

  @param  RtTable               The route table to remove the route from
  @param  Dest                  The destination network
  @param  Netmask               The netmask of the Dest
  @param  Gateway               The next hop address

  @retval EFI_SUCCESS           The route entry is successfully removed
  @retval EFI_NOT_FOUND         There is no route entry in the table with that
                                properity.

**/
EFI_STATUS
Ip4DelRoute (
  IN IP4_ROUTE_TABLE      *RtTable,
  IN IP4_ADDR             Dest,
  IN IP4_ADDR             Netmask,
  IN IP4_ADDR             Gateway
  )
{
  LIST_ENTRY                *Head;
  LIST_ENTRY                *Entry;
  LIST_ENTRY                *Next;
  IP4_ROUTE_ENTRY           *RtEntry;

  Head = &(RtTable->RouteArea[NetGetMaskLength (Netmask)]);

  NET_LIST_FOR_EACH_SAFE (Entry, Next, Head) {
    RtEntry = NET_LIST_USER_STRUCT (Entry, IP4_ROUTE_ENTRY, Link);

    if (IP4_NET_EQUAL (RtEntry->Dest, Dest, Netmask) && (RtEntry->NextHop == Gateway)) {
      Ip4PurgeRouteCache (&RtTable->Cache, (UINTN) RtEntry);
      RemoveEntryList (Entry);
      Ip4FreeRouteEntry  (RtEntry);

      RtTable->TotalNum--;
      return EFI_SUCCESS;
    }
  }

  return EFI_NOT_FOUND;
}


/**
  Find a route cache with the dst and src. This is used by ICMP
  redirect messasge process. All kinds of redirect is treated as
  host redirect according to RFC1122. So, only route cache entries
  are modified according to the ICMP redirect message.

  @param  RtTable               The route table to search the cache for
  @param  Dest                  The destination address
  @param  Src                   The source address

  @return NULL if no route entry to the (Dest, Src). Otherwise the point
  @return to the correct route cache entry.

**/
IP4_ROUTE_CACHE_ENTRY *
Ip4FindRouteCache (
  IN IP4_ROUTE_TABLE        *RtTable,
  IN IP4_ADDR               Dest,
  IN IP4_ADDR               Src
  )
{
  LIST_ENTRY                *Entry;
  IP4_ROUTE_CACHE_ENTRY     *RtCacheEntry;
  UINT32                    Index;

  Index = IP4_ROUTE_CACHE_HASH (Dest, Src);

  NET_LIST_FOR_EACH (Entry, &RtTable->Cache.CacheBucket[Index]) {
    RtCacheEntry = NET_LIST_USER_STRUCT (Entry, IP4_ROUTE_CACHE_ENTRY, Link);

    if ((RtCacheEntry->Dest == Dest) && (RtCacheEntry->Src == Src)) {
      NET_GET_REF (RtCacheEntry);
      return RtCacheEntry;
    }
  }

  return NULL;
}


/**
  Search the route table for a most specific match to the Dst. It searches
  from the longest route area (mask length == 32) to the shortest route area (
  default routes). In each route area, it will first search the instance's
  route table, then the default route table. This is required by the following
  requirements:
  1. IP search the route table for a most specific match
  2. The local route entries have precedence over the default route entry.

  @param  RtTable               The route table to search from
  @param  Dst                   The destionation address to search

  @return NULL if no route matches the Dst, otherwise the point to the
  @return most specific route to the Dst.

**/
STATIC
IP4_ROUTE_ENTRY *
Ip4FindRouteEntry (
  IN IP4_ROUTE_TABLE        *RtTable,
  IN IP4_ADDR               Dst
  )
{
  LIST_ENTRY                *Entry;
  IP4_ROUTE_ENTRY           *RtEntry;
  IP4_ROUTE_TABLE           *Table;
  INTN                      Index;

  RtEntry = NULL;

  for (Index = IP4_MASK_NUM - 1; Index >= 0; Index--) {
    for (Table = RtTable; Table != NULL; Table = Table->Next) {
      NET_LIST_FOR_EACH (Entry, &Table->RouteArea[Index]) {
        RtEntry = NET_LIST_USER_STRUCT (Entry, IP4_ROUTE_ENTRY, Link);

        if (IP4_NET_EQUAL (RtEntry->Dest, Dst, RtEntry->Netmask)) {
          NET_GET_REF (RtEntry);
          return RtEntry;
        }
      }
    }
  }


  return NULL;
}


/**
  Search the route table to route the packet. Return/creat a route
  cache if there is a route to the destination.

  @param  RtTable               The route table to search from
  @param  Dest                  The destination address to search for
  @param  Src                   The source address to search for

  @return NULL if failed to route packet, otherwise a route cache
  @return entry that can be used to route packet.

**/
IP4_ROUTE_CACHE_ENTRY *
Ip4Route (
  IN IP4_ROUTE_TABLE        *RtTable,
  IN IP4_ADDR               Dest,
  IN IP4_ADDR               Src
  )
{
  LIST_ENTRY                *Head;
  LIST_ENTRY                *Entry;
  LIST_ENTRY                *Next;
  IP4_ROUTE_CACHE_ENTRY     *RtCacheEntry;
  IP4_ROUTE_CACHE_ENTRY     *Cache;
  IP4_ROUTE_ENTRY           *RtEntry;
  IP4_ADDR                  NextHop;
  UINT32                    Count;

  ASSERT (RtTable != NULL);

  Head          = &RtTable->Cache.CacheBucket[IP4_ROUTE_CACHE_HASH (Dest, Src)];
  RtCacheEntry  = Ip4FindRouteCache (RtTable, Dest, Src);

  //
  // If found, promote the cache entry to the head of the hash bucket. LRU
  //
  if (RtCacheEntry != NULL) {
    RemoveEntryList (&RtCacheEntry->Link);
    InsertHeadList (Head, &RtCacheEntry->Link);
    return RtCacheEntry;
  }

  //
  // Search the route table for the most specific route
  //
  RtEntry = Ip4FindRouteEntry (RtTable, Dest);

  if (RtEntry == NULL) {
    return NULL;
  }

  //
  // Found a route to the Dest, if it is a direct route, the packet
  // will be send directly to the destination, such as for connected
  // network. Otherwise, it is an indirect route, the packet will be
  // send the next hop router.
  //
  if (RtEntry->Flag & IP4_DIRECT_ROUTE) {
    NextHop = Dest;
  } else {
    NextHop = RtEntry->NextHop;
  }

  Ip4FreeRouteEntry (RtEntry);

  //
  // Create a route cache entry, and tag it as spawned from this route entry
  //
  RtCacheEntry = Ip4CreateRouteCacheEntry (Dest, Src, NextHop, (UINTN) RtEntry);

  if (RtCacheEntry == NULL) {
    return NULL;
  }

  InsertHeadList (Head, &RtCacheEntry->Link);
  NET_GET_REF (RtCacheEntry);

  //
  // Each bucket of route cache can contain at most 64 entries.
  // Remove the entries at the tail of the bucket. These entries
  // are likely to be used least.
  //
  Count = 0;
  NET_LIST_FOR_EACH_SAFE (Entry, Next, Head) {
    if (++Count < IP4_ROUTE_CACHE_MAX) {
      continue;
    }

    Cache = NET_LIST_USER_STRUCT (Entry, IP4_ROUTE_CACHE_ENTRY, Link);

    RemoveEntryList (Entry);
    Ip4FreeRouteCacheEntry (Cache);
  }

  return RtCacheEntry;
}


/**
  Build a EFI_IP4_ROUTE_TABLE to be returned to the caller of
  GetModeData. The EFI_IP4_ROUTE_TABLE is clumsy to use in the
  internal operation of the IP4 driver.

  @param  IpInstance            The IP4 child that requests the route table.

  @retval EFI_SUCCESS           The route table is successfully build
  @retval EFI_OUT_OF_RESOURCES  Failed to allocate the memory for the rotue table.

**/
EFI_STATUS
Ip4BuildEfiRouteTable (
  IN IP4_PROTOCOL           *IpInstance
  )
{
  LIST_ENTRY                *Entry;
  IP4_ROUTE_TABLE           *RtTable;
  IP4_ROUTE_ENTRY           *RtEntry;
  EFI_IP4_ROUTE_TABLE       *Table;
  UINT32                    Count;
  INT32                     Index;

  RtTable = IpInstance->RouteTable;

  if (IpInstance->EfiRouteTable != NULL) {
    gBS->FreePool (IpInstance->EfiRouteTable);

    IpInstance->EfiRouteTable = NULL;
    IpInstance->EfiRouteCount = 0;
  }

  Count = RtTable->TotalNum;

  if (RtTable->Next != NULL) {
    Count += RtTable->Next->TotalNum;
  }

  if (Count == 0) {
    return EFI_SUCCESS;
  }

  Table = AllocatePool (sizeof (EFI_IP4_ROUTE_TABLE) * Count);

  if (Table == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  //
  // Copy the route entry to EFI route table. Keep the order of
  // route entry copied from most specific to default route. That
  // is, interlevel the route entry from the instance's route area
  // and those from the default route table's route area.
  //
  Count = 0;

  for (Index = IP4_MASK_NUM - 1; Index >= 0; Index--) {
    for (RtTable = IpInstance->RouteTable; RtTable != NULL; RtTable = RtTable->Next) {
      NET_LIST_FOR_EACH (Entry, &(RtTable->RouteArea[Index])) {
        RtEntry = NET_LIST_USER_STRUCT (Entry, IP4_ROUTE_ENTRY, Link);

        EFI_IP4 (Table[Count].SubnetAddress)  = HTONL (RtEntry->Dest & RtEntry->Netmask);
        EFI_IP4 (Table[Count].SubnetMask)     = HTONL (RtEntry->Netmask);
        EFI_IP4 (Table[Count].GatewayAddress) = HTONL (RtEntry->NextHop);

        Count++;
      }
    }
  }

  IpInstance->EfiRouteTable = Table;
  IpInstance->EfiRouteCount = Count;
  return EFI_SUCCESS;
}
