/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2025 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef HISTORY_H_INCLUDED
#define HISTORY_H_INCLUDED

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <type_traits>  // IWYU pragma: keep

#include "misc.h"
#include "position.h"
#include "bitboard.h"

namespace Stockfish {

constexpr int PAWN_HISTORY_SIZE        = 512;    // has to be a power of 2
constexpr int CORRECTION_HISTORY_SIZE  = 32768;  // has to be a power of 2
constexpr int CORRECTION_HISTORY_LIMIT = 1024;
constexpr int LOW_PLY_HISTORY_SIZE     = 5;

static_assert((PAWN_HISTORY_SIZE & (PAWN_HISTORY_SIZE - 1)) == 0,
              "PAWN_HISTORY_SIZE has to be a power of 2");

static_assert((CORRECTION_HISTORY_SIZE & (CORRECTION_HISTORY_SIZE - 1)) == 0,
              "CORRECTION_HISTORY_SIZE has to be a power of 2");

enum PawnHistoryType {
    Normal,
    Correction
};

constexpr uint64_t KnightMagic[64] =
{2649526798775546678ULL,4152603468059905820ULL,4323738553495348598ULL,8358822318513263564ULL,
8233741379241091944ULL,15862258506660595117ULL,11034153426427387283ULL,5179342299017078485ULL,
13953775647861833869ULL,9285332830472462433ULL,4707670090173510570ULL,633188408852161521ULL,
9295575317119436597ULL,2602449537074926626ULL,10402735044161298543ULL,5209354341409342328ULL,
162625749878507151ULL,1009651158123930541ULL,17868030413482491775ULL,18157387793351835647ULL,
18428448182976380927ULL,15559866339509124062ULL,15570116201671053920ULL,1155280919795624321ULL,
2486129120195482830ULL,13187947271813240921ULL,18406210577484414942ULL,18427602675714473387ULL,
18441114298743767039ULL,13816761900458831871ULL,6080440195611631637ULL,17058553541789489427ULL,
16801810908685894692ULL,10863528935562739968ULL,18302056847512170491ULL,13832238890018011135ULL,
18441604947771359199ULL,4539399656176549631ULL,10034574284689510417ULL,6922771568222818337ULL,
4170423972204183689ULL,13981410758149154305ULL,1458212938463817744ULL,14940013606182964225ULL,
10126812077910748164ULL,14499197477661460225ULL,10889334102842943525ULL,11449759955925008433ULL,
7662239901190162178ULL,10624671891104776129ULL,17829790316069208625ULL,834552698710835841ULL,
1569976736693633153ULL,18290846858870916161ULL,12326740164105761617ULL,5482981632183385993ULL,
9386590720569999446ULL,5202246693298479759ULL,15779688972541825106ULL,13366487713771978889ULL,
11198365102992539725ULL,15043574735303614497ULL,18334953575009493081ULL,975649118817374295ULL};
//This is a collision-free
inline int knight_attack_index(Bitboard target, Square from)
{
    return (target*KnightMagic[from])>>56;
}

template<PawnHistoryType T = Normal>
inline int pawn_structure_index(const Position& pos) {
    return pos.pawn_key() & ((T == Normal ? PAWN_HISTORY_SIZE : CORRECTION_HISTORY_SIZE) - 1);
}

inline int minor_piece_index(const Position& pos) {
    return pos.minor_piece_key() & (CORRECTION_HISTORY_SIZE - 1);
}

template<Color c>
inline int non_pawn_index(const Position& pos) {
    return pos.non_pawn_key(c) & (CORRECTION_HISTORY_SIZE - 1);
}

// StatsEntry is the container of various numerical statistics. We use a class
// instead of a naked value to directly call history update operator<<() on
// the entry. The first template parameter T is the base type of the array,
// and the second template parameter D limits the range of updates in [-D, D]
// when we update values with the << operator
template<typename T, int D>
class StatsEntry {

    static_assert(std::is_arithmetic_v<T>, "Not an arithmetic type");
    static_assert(D <= std::numeric_limits<T>::max(), "D overflows T");

    T entry;

   public:
    StatsEntry& operator=(const T& v) {
        entry = v;
        return *this;
    }
    operator const T&() const { return entry; }

    void operator<<(int bonus) {
        // Make sure that bonus is in range [-D, D]
        int clampedBonus = std::clamp(bonus, -D, D);
        entry += clampedBonus - entry * std::abs(clampedBonus) / D;

        assert(std::abs(entry) <= D);
    }
};

enum StatsType {
    NoCaptures,
    Captures
};

template<typename T, int D, std::size_t... Sizes>
using Stats = MultiArray<StatsEntry<T, D>, Sizes...>;

// ButterflyHistory records how often quiet moves have been successful or unsuccessful
// during the current search, and is used for reduction and move ordering decisions.
// It uses 2 tables (one for each color) indexed by the move's from and to squares,
// see https://www.chessprogramming.org/Butterfly_Boards (~11 elo)
using ButterflyHistory = Stats<std::int16_t, 7183, COLOR_NB, int(SQUARE_NB) * int(SQUARE_NB)>;

// LowPlyHistory is adressed by play and move's from and to squares, used
// to improve move ordering near the root
using LowPlyHistory =
  Stats<std::int16_t, 7183, LOW_PLY_HISTORY_SIZE, int(SQUARE_NB) * int(SQUARE_NB)>;

// CapturePieceToHistory is addressed by a move's [piece][to][captured piece type]
using CapturePieceToHistory = Stats<std::int16_t, 10692, PIECE_NB, SQUARE_NB, PIECE_TYPE_NB>;

// PieceToHistory is like ButterflyHistory but is addressed by a move's [piece][to]
using PieceToHistory = Stats<std::int16_t, 30000, PIECE_NB, SQUARE_NB>;

// ContinuationHistory is the combined history of a given pair of moves, usually
// the current one given a previous one. The nested history table is based on
// PieceToHistory instead of ButterflyBoards.
// (~63 elo)
using ContinuationHistory = MultiArray<PieceToHistory, PIECE_NB, SQUARE_NB>;

// PawnHistory is addressed by the pawn structure and a move's [piece][to]
using PawnHistory = Stats<std::int16_t, 8192, PAWN_HISTORY_SIZE, PIECE_NB, SQUARE_NB>;

using KnightHistory = Stats<std::int16_t, 5000,COLOR_NB, SQUARE_NB, 256>;

// Correction histories record differences between the static evaluation of
// positions and their search score. It is used to improve the static evaluation
// used by some search heuristics.
// see https://www.chessprogramming.org/Static_Evaluation_Correction_History
enum CorrHistType {
    Pawn,          // By color and pawn structure
    Minor,         // By color and positions of minor pieces (Knight, Bishop)
    NonPawn,       // By non-pawn material positions and color
    PieceTo,       // By [piece][to] move
    Continuation,  // Combined history of move pairs
};

namespace Detail {

template<CorrHistType>
struct CorrHistTypedef {
    using type = Stats<std::int16_t, CORRECTION_HISTORY_LIMIT, CORRECTION_HISTORY_SIZE, COLOR_NB>;
};

template<>
struct CorrHistTypedef<PieceTo> {
    using type = Stats<std::int16_t, CORRECTION_HISTORY_LIMIT, PIECE_NB, SQUARE_NB>;
};

template<>
struct CorrHistTypedef<Continuation> {
    using type = MultiArray<CorrHistTypedef<PieceTo>::type, PIECE_NB, SQUARE_NB>;
};

template<>
struct CorrHistTypedef<NonPawn> {
    using type =
      Stats<std::int16_t, CORRECTION_HISTORY_LIMIT, CORRECTION_HISTORY_SIZE, COLOR_NB, COLOR_NB>;
};

}

template<CorrHistType T>
using CorrectionHistory = typename Detail::CorrHistTypedef<T>::type;

using TTMoveHistory = StatsEntry<std::int16_t, 8192>;

}  // namespace Stockfish

#endif  // #ifndef HISTORY_H_INCLUDED
