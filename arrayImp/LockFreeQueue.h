/* =============================================
 * Copyright (C) 2020-2020
 * Author : bhlzlx@gmail.com
 * 
 * 其实也是参考网上的代码写的，不过感觉别人写的有问题
 * 自己也改了一些，然后加了注释的，写一堆多线程代码
 * 再不给逻辑加注释，那不太好。
 * ============================================*/

#pragma once

#include <atomic>
#include <cstdint>

namespace ugi {

    template < class T >
    class LockFreeQueue {
        static constexpr size_t InvalidPosition = ~0ULL;        //!> a constant value for reference
        static constexpr size_t CacheLineAlignmentSize = 64 - sizeof(size_t);
    private:
        struct Node {
            T                   val;
            std::atomic<size_t> readIndex;                      //!> readIndex is the value for get the status of read
            std::atomic<size_t> storeIndex;                     //!> storeIndex is the value of ( index + capacity*cycle )
        };

        Node*                   _nodeArray;                     //!> stores all nodes for the queue
        size_t                  _capacity;                      //!> the capacity of the queue
        size_t                  _capacityMask;                  //!> a utility value for location the index of the '_nodeArray'

        std::atomic<size_t>     _head;                          //!> head of the queue
        char                    _align[CacheLineAlignmentSize]; //!> cache line aligment, optimize for multi-core processors
        std::atomic<size_t>     _tail;                          //!> tail of the queue


    public:
        LockFreeQueue( size_t capacity )
            : _nodeArray( nullptr )
            , _capacity(0)
            , _capacityMask(0)
            , _head(0)
            , _tail(0)
        {
            size_t capMaskRef = capacity - 1;
            // set _capacityMask
            while(capMaskRef) {
                _capacityMask |= capMaskRef;
                capMaskRef >>=1;
            }
            _capacity = _capacityMask + 1;
            _nodeArray = (Node*)( new uint8_t[ sizeof(Node) * _capacity ] );
            _head.store(0, std::memory_order_relaxed);
            _tail.store(0, std::memory_order_relaxed);
            //
            for( size_t i = 0; i<_capacity; ++i ) {
                _nodeArray[i].readIndex.store( InvalidPosition, std::memory_order_relaxed );        //> store a value that will never equal to '_head'
                _nodeArray[i].storeIndex.store( i, std::memory_order_relaxed );                     //> store a value for first cycle( pre-allocate for first 'store' cycle )
            }
        }

        bool push( T val )  
        {
            Node* node = nullptr;
            size_t tailCapture = 0;
            while(true) {
                tailCapture = _tail.load( std::memory_order_relaxed );                              //> get the capture of '_tail' position
                node = &_nodeArray[tailCapture&_capacityMask];                                      //> get the pointer of the node
                size_t storeIndexCapture = node->storeIndex.load( std::memory_order_relaxed );      //> get the node's 'storeIndex'
                /* ==============================================================================
                 * when we stores a new value, the node's storeIndex must be equal to '_tail', 
                 * otherwise, '_tail' and storeIndexCapture are in different cycle (the queue is full!)
                 * so, it it failed, we should return 'false' immediately
                 * ============================================================================ */
                if( storeIndexCapture != tailCapture ) {                                              
                    return false;
                }
                // update '_tail'
                if( _tail.compare_exchange_weak( tailCapture, tailCapture + 1, std::memory_order_relaxed ) ) {
                    break;
                }
            }
            new(&node->val)T(val);
            /* ==================================================================
             *   make the fcuntion call failed if we have not assign the value in the situation
             * that the queue only has the current element
             *   release to ensure constrcuction of Node::val not be reordered after call of store
             * ================================================================== */
            node->readIndex.store( tailCapture, std::memory_order_release ); 
            return true;
        }

        bool pop( T& val ) 
        {
            Node* node = nullptr;
            size_t headCapture = 0;
            while(true) {
                headCapture = _head.load( std::memory_order_relaxed );
                node = &_nodeArray[headCapture&_capacityMask];
                size_t readIndex = node->readIndex.load( std::memory_order_relaxed );
                /* ==================================================================
                 * similar to store operation, if readIndex not equal to '_head',
                 * readIndex & _head are in different cycle, the queue is empty
                 * ==================================================================*/
                if( readIndex != headCapture) {
                    return false;
                }
                if( _head.compare_exchange_weak( headCapture, headCapture+1, std::memory_order_relaxed ) ) {
                    break;
                }
            }
            val = std::move(node->val);
            (&node->val)->~T();
            // update the storeIndex for next cycle's store operation
            node->storeIndex.store( headCapture + _capacity, std::memory_order_release );
            return true;
        }
    };

}