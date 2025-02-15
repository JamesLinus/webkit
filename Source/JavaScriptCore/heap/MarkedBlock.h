/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2011, 2016 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef MarkedBlock_h
#define MarkedBlock_h

#include "AllocatorAttributes.h"
#include "DestructionMode.h"
#include "HeapCell.h"
#include "HeapOperation.h"
#include "IterationStatus.h"
#include "WeakSet.h"
#include <wtf/Bitmap.h>
#include <wtf/DataLog.h>
#include <wtf/DoublyLinkedList.h>
#include <wtf/HashFunctions.h>
#include <wtf/StdLibExtras.h>

// Set to log state transitions of blocks.
#define HEAP_LOG_BLOCK_STATE_TRANSITIONS 0

#if HEAP_LOG_BLOCK_STATE_TRANSITIONS
#define HEAP_LOG_BLOCK_STATE_TRANSITION(block) do {                     \
        dataLogF(                                                    \
            "%s:%d %s: block %s = %p, %d\n",                            \
            __FILE__, __LINE__, __FUNCTION__,                           \
            #block, (block), (block)->m_state);                         \
    } while (false)
#else
#define HEAP_LOG_BLOCK_STATE_TRANSITION(block) ((void)0)
#endif

namespace JSC {
    
    class Heap;
    class JSCell;
    class MarkedAllocator;

    typedef uintptr_t Bits;

    // A marked block is a page-aligned container for heap-allocated objects.
    // Objects are allocated within cells of the marked block. For a given
    // marked block, all cells have the same size. Objects smaller than the
    // cell size may be allocated in the marked block, in which case the
    // allocation suffers from internal fragmentation: wasted space whose
    // size is equal to the difference between the cell size and the object
    // size.

    class MarkedBlock : public DoublyLinkedListNode<MarkedBlock> {
        friend class WTF::DoublyLinkedListNode<MarkedBlock>;
        friend class LLIntOffsetsExtractor;
        friend struct VerifyMarkedOrRetired;
    public:
        static const size_t atomSize = 16; // bytes
        static const size_t blockSize = 16 * KB;
        static const size_t blockMask = ~(blockSize - 1); // blockSize must be a power of two.

        static const size_t atomsPerBlock = blockSize / atomSize;

        static_assert(!(MarkedBlock::atomSize & (MarkedBlock::atomSize - 1)), "MarkedBlock::atomSize must be a power of two.");
        static_assert(!(MarkedBlock::blockSize & (MarkedBlock::blockSize - 1)), "MarkedBlock::blockSize must be a power of two.");

        struct FreeCell {
            FreeCell* next;
        };
        
        struct FreeList {
            FreeCell* head;
            size_t bytes;

            FreeList();
            FreeList(FreeCell*, size_t);
        };

        struct VoidFunctor {
            typedef void ReturnType;
            void returnValue() { }
        };

        class CountFunctor {
        public:
            typedef size_t ReturnType;

            CountFunctor() : m_count(0) { }
            void count(size_t count) const { m_count += count; }
            ReturnType returnValue() const { return m_count; }

        private:
            // FIXME: This is mutable because we're using a functor rather than C++ lambdas.
            // https://bugs.webkit.org/show_bug.cgi?id=159644
            mutable ReturnType m_count;
        };

        static MarkedBlock* create(Heap&, MarkedAllocator*, size_t capacity, size_t cellSize, const AllocatorAttributes&);
        static void destroy(Heap&, MarkedBlock*);

        static bool isAtomAligned(const void*);
        static MarkedBlock* blockFor(const void*);
        static size_t firstAtom();
        
        void lastChanceToFinalize();

        MarkedAllocator* allocator() const;
        Heap* heap() const;
        VM* vm() const;
        WeakSet& weakSet();
        
        enum SweepMode { SweepOnly, SweepToFreeList };
        FreeList sweep(SweepMode = SweepOnly);

        void shrink();

        void visitWeakSet(HeapRootVisitor&);
        void reapWeakSet();

        // While allocating from a free list, MarkedBlock temporarily has bogus
        // cell liveness data. To restore accurate cell liveness data, call one
        // of these functions:
        void didConsumeFreeList(); // Call this once you've allocated all the items in the free list.
        void stopAllocating(const FreeList&);
        FreeList resumeAllocating(); // Call this if you canonicalized a block for some non-collection related purpose.

        // Returns true if the "newly allocated" bitmap was non-null 
        // and was successfully cleared and false otherwise.
        bool clearNewlyAllocated();
        void clearMarks();
        template <HeapOperation collectionType>
        void clearMarksWithCollectionType();

        size_t markCount();
        bool isEmpty();

        size_t cellSize();
        const AllocatorAttributes& attributes() const;
        DestructionMode destruction() const;
        bool needsDestruction() const;
        HeapCell::Kind cellKind() const;

        size_t size();
        size_t capacity();

        bool isMarked(const void*);
        bool testAndSetMarked(const void*);
        bool isLive(const HeapCell*);
        bool isLiveCell(const void*);
        bool isAtom(const void*);
        bool isMarkedOrNewlyAllocated(const HeapCell*);
        void setMarked(const void*);
        void clearMarked(const void*);

        bool isNewlyAllocated(const void*);
        void setNewlyAllocated(const void*);
        void clearNewlyAllocated(const void*);

        bool isAllocated() const;
        bool isMarkedOrRetired() const;
        bool needsSweeping() const;
        void didRetireBlock(const FreeList&);
        void willRemoveBlock();

        template <typename Functor> IterationStatus forEachCell(const Functor&);
        template <typename Functor> IterationStatus forEachLiveCell(const Functor&);
        template <typename Functor> IterationStatus forEachDeadCell(const Functor&);

    private:
        static const size_t atomAlignmentMask = atomSize - 1;

        enum BlockState { New, FreeListed, Allocated, Marked, Retired };
        template<bool callDestructors> FreeList sweepHelper(SweepMode = SweepOnly);

        typedef char Atom[atomSize];

        MarkedBlock(MarkedAllocator*, size_t capacity, size_t cellSize, const AllocatorAttributes&);
        Atom* atoms();
        size_t atomNumber(const void*);
        void callDestructor(HeapCell*);
        template<BlockState, SweepMode, bool callDestructors> FreeList specializedSweep();
        
        MarkedBlock* m_prev;
        MarkedBlock* m_next;

        size_t m_atomsPerCell;
        size_t m_endAtom; // This is a fuzzy end. Always test for < m_endAtom.
        WTF::Bitmap<atomsPerBlock, WTF::BitmapAtomic, uint8_t> m_marks;
        std::unique_ptr<WTF::Bitmap<atomsPerBlock>> m_newlyAllocated;

        size_t m_capacity;
        AllocatorAttributes m_attributes;
        MarkedAllocator* m_allocator;
        BlockState m_state;
        WeakSet m_weakSet;
    };

    inline MarkedBlock::FreeList::FreeList()
        : head(0)
        , bytes(0)
    {
    }

    inline MarkedBlock::FreeList::FreeList(FreeCell* head, size_t bytes)
        : head(head)
        , bytes(bytes)
    {
    }

    inline size_t MarkedBlock::firstAtom()
    {
        return WTF::roundUpToMultipleOf<atomSize>(sizeof(MarkedBlock)) / atomSize;
    }

    inline MarkedBlock::Atom* MarkedBlock::atoms()
    {
        return reinterpret_cast<Atom*>(this);
    }

    inline bool MarkedBlock::isAtomAligned(const void* p)
    {
        return !(reinterpret_cast<Bits>(p) & atomAlignmentMask);
    }

    inline MarkedBlock* MarkedBlock::blockFor(const void* p)
    {
        return reinterpret_cast<MarkedBlock*>(reinterpret_cast<Bits>(p) & blockMask);
    }

    inline MarkedAllocator* MarkedBlock::allocator() const
    {
        return m_allocator;
    }

    inline Heap* MarkedBlock::heap() const
    {
        return m_weakSet.heap();
    }

    inline VM* MarkedBlock::vm() const
    {
        return m_weakSet.vm();
    }

    inline WeakSet& MarkedBlock::weakSet()
    {
        return m_weakSet;
    }

    inline void MarkedBlock::shrink()
    {
        m_weakSet.shrink();
    }

    inline void MarkedBlock::visitWeakSet(HeapRootVisitor& heapRootVisitor)
    {
        m_weakSet.visit(heapRootVisitor);
    }

    inline void MarkedBlock::reapWeakSet()
    {
        m_weakSet.reap();
    }

    inline void MarkedBlock::willRemoveBlock()
    {
        ASSERT(m_state != Retired);
    }

    inline void MarkedBlock::didConsumeFreeList()
    {
        HEAP_LOG_BLOCK_STATE_TRANSITION(this);

        ASSERT(m_state == FreeListed);
        m_state = Allocated;
    }

    inline size_t MarkedBlock::markCount()
    {
        return m_marks.count();
    }

    inline bool MarkedBlock::isEmpty()
    {
        return m_marks.isEmpty() && m_weakSet.isEmpty() && (!m_newlyAllocated || m_newlyAllocated->isEmpty());
    }

    inline size_t MarkedBlock::cellSize()
    {
        return m_atomsPerCell * atomSize;
    }

    inline const AllocatorAttributes& MarkedBlock::attributes() const
    {
        return m_attributes;
    }

    inline bool MarkedBlock::needsDestruction() const
    {
        return m_attributes.destruction == NeedsDestruction;
    }

    inline DestructionMode MarkedBlock::destruction() const
    {
        return m_attributes.destruction;
    }

    inline HeapCell::Kind MarkedBlock::cellKind() const
    {
        return m_attributes.cellKind;
    }

    inline size_t MarkedBlock::size()
    {
        return markCount() * cellSize();
    }

    inline size_t MarkedBlock::capacity()
    {
        return m_capacity;
    }

    inline size_t MarkedBlock::atomNumber(const void* p)
    {
        return (reinterpret_cast<Bits>(p) - reinterpret_cast<Bits>(this)) / atomSize;
    }

    inline bool MarkedBlock::isMarked(const void* p)
    {
        return m_marks.get(atomNumber(p));
    }

    inline bool MarkedBlock::testAndSetMarked(const void* p)
    {
        return m_marks.concurrentTestAndSet(atomNumber(p));
    }

    inline void MarkedBlock::setMarked(const void* p)
    {
        m_marks.set(atomNumber(p));
    }

    inline void MarkedBlock::clearMarked(const void* p)
    {
        ASSERT(m_marks.get(atomNumber(p)));
        m_marks.clear(atomNumber(p));
    }

    inline bool MarkedBlock::isNewlyAllocated(const void* p)
    {
        return m_newlyAllocated->get(atomNumber(p));
    }

    inline void MarkedBlock::setNewlyAllocated(const void* p)
    {
        m_newlyAllocated->set(atomNumber(p));
    }

    inline void MarkedBlock::clearNewlyAllocated(const void* p)
    {
        m_newlyAllocated->clear(atomNumber(p));
    }

    inline bool MarkedBlock::clearNewlyAllocated()
    {
        if (m_newlyAllocated) {
            m_newlyAllocated = nullptr;
            return true;
        }
        return false;
    }

    inline bool MarkedBlock::isMarkedOrNewlyAllocated(const HeapCell* cell)
    {
        ASSERT(m_state == Retired || m_state == Marked);
        return m_marks.get(atomNumber(cell)) || (m_newlyAllocated && isNewlyAllocated(cell));
    }

    inline bool MarkedBlock::isLive(const HeapCell* cell)
    {
        switch (m_state) {
        case Allocated:
            return true;

        case Retired:
        case Marked:
            return isMarkedOrNewlyAllocated(cell);

        case New:
        case FreeListed:
            RELEASE_ASSERT_NOT_REACHED();
            return false;
        }

        RELEASE_ASSERT_NOT_REACHED();
        return false;
    }

    inline bool MarkedBlock::isAtom(const void* p)
    {
        ASSERT(MarkedBlock::isAtomAligned(p));
        size_t atomNumber = this->atomNumber(p);
        size_t firstAtom = this->firstAtom();
        if (atomNumber < firstAtom) // Filters pointers into MarkedBlock metadata.
            return false;
        if ((atomNumber - firstAtom) % m_atomsPerCell) // Filters pointers into cell middles.
            return false;
        if (atomNumber >= m_endAtom) // Filters pointers into invalid cells out of the range.
            return false;
        return true;
    }

    inline bool MarkedBlock::isLiveCell(const void* p)
    {
        if (!isAtom(p))
            return false;
        return isLive(static_cast<const HeapCell*>(p));
    }

    template <typename Functor> inline IterationStatus MarkedBlock::forEachCell(const Functor& functor)
    {
        HeapCell::Kind kind = m_attributes.cellKind;
        for (size_t i = firstAtom(); i < m_endAtom; i += m_atomsPerCell) {
            HeapCell* cell = reinterpret_cast_ptr<HeapCell*>(&atoms()[i]);
            if (functor(cell, kind) == IterationStatus::Done)
                return IterationStatus::Done;
        }
        return IterationStatus::Continue;
    }

    template <typename Functor> inline IterationStatus MarkedBlock::forEachLiveCell(const Functor& functor)
    {
        HeapCell::Kind kind = m_attributes.cellKind;
        for (size_t i = firstAtom(); i < m_endAtom; i += m_atomsPerCell) {
            HeapCell* cell = reinterpret_cast_ptr<HeapCell*>(&atoms()[i]);
            if (!isLive(cell))
                continue;

            if (functor(cell, kind) == IterationStatus::Done)
                return IterationStatus::Done;
        }
        return IterationStatus::Continue;
    }

    template <typename Functor> inline IterationStatus MarkedBlock::forEachDeadCell(const Functor& functor)
    {
        HeapCell::Kind kind = m_attributes.cellKind;
        for (size_t i = firstAtom(); i < m_endAtom; i += m_atomsPerCell) {
            HeapCell* cell = reinterpret_cast_ptr<HeapCell*>(&atoms()[i]);
            if (isLive(cell))
                continue;

            if (functor(cell, kind) == IterationStatus::Done)
                return IterationStatus::Done;
        }
        return IterationStatus::Continue;
    }

    inline bool MarkedBlock::needsSweeping() const
    {
        return m_state == Marked;
    }

    inline bool MarkedBlock::isAllocated() const
    {
        return m_state == Allocated;
    }

    inline bool MarkedBlock::isMarkedOrRetired() const
    {
        return m_state == Marked || m_state == Retired;
    }

} // namespace JSC

namespace WTF {

    struct MarkedBlockHash : PtrHash<JSC::MarkedBlock*> {
        static unsigned hash(JSC::MarkedBlock* const& key)
        {
            // Aligned VM regions tend to be monotonically increasing integers,
            // which is a great hash function, but we have to remove the low bits,
            // since they're always zero, which is a terrible hash function!
            return reinterpret_cast<JSC::Bits>(key) / JSC::MarkedBlock::blockSize;
        }
    };

    template<> struct DefaultHash<JSC::MarkedBlock*> {
        typedef MarkedBlockHash Hash;
    };

} // namespace WTF

#endif // MarkedBlock_h
