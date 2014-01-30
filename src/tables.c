/* $Id: tables.c,v 1.10 2005-02-16 21:08:49 martin Exp $ */

#include "tables.h"

table_t knight_position = {
  -15,-10,-6,-5,-5,-6,-10,-15,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,
  -8,-7,-2,+0,+0,-2,-7,-8,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,
  -5,-2,+5,+5,+5,+5,-2,-5,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,
  -4,+0,+5,+10,+10,+5,+0,-4,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,
  -4,+0,+5,+10,+10,+5,+0,-4,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,
  -5,-2,+5,+5,+5,+5,-2,-5,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,
  -8,-7,-2,+0,+0,-2,-7,-8,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,
  -15,-10,-6,-5,-5,-6,-10,-15,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD
};

table_t bishop_position = {
  -3,-2,-1,-1,-1,-1,-2,-3,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,
  -2,+1,+0,+0,+0,+0,+1,-2,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,
  -1,+0,+1,+1,+1,+1,+0,-1,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,
  -1,+0,+1,+2,+2,+1,+0,-1,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,
  -1,+0,+1,+2,+2,+1,+0,-1,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,
  -1,+0,+1,+1,+1,+1,+0,-1,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,
  -2,+1,+0,+0,+0,+0,+1,-2,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,
  -3,-2,-1,-1,-1,-1,-2,-3,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD
};

table_t queen_position = {
  -5,-4,-3,+0,-1,-3,-4,-5,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,
  -4,-3,+0,+1,+1,+0,-3,-4,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,
  -3,+0,+1,+1,+1,+1,+0,-3,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,
  -1,+0,+1,+1,+1,+1,+0,-1,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,
  -1,+0,+1,+1,+1,+1,+0,-1,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,
  -3,+0,+1,+1,+1,+1,+0,-3,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,
  -4,-3,+0,+1,+1,+0,-3,-4,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,
  -5,-4,-3,+0,-1,-3,-4,-5,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD
};

table_t white_pawn_position = {
  BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,
  +0,+0,+0,-1,-1,+0,+0,+0,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,
  +0,+0,+0,+1,+1,+0,+0,+0,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,
  +0,+0,+0,+3,+3,+0,+0,+0,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,
  +0,+0,+0,+3,+3,+0,+0,+0,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,
  +3,+3,+3,+3,+3,+3,+3,+3,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,
  +5,+5,+5,+5,+5,+5,+5,+5,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,
  BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD
};

table_t black_pawn_position = {
  BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,
  +5,+5,+5,+5,+5,+5,+5,+5,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,
  +3,+3,+3,+3,+3,+3,+3,+3,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,
  +0,+0,+0,+3,+3,+0,+0,+0,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,
  +0,+0,+0,+3,+3,+0,+0,+0,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,
  +0,+0,+0,+1,+1,+0,+0,+0,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,
  +0,+0,+0,-1,-1,+0,+0,+0,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,
  BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD
};

table_t king_eg_position = {
  +0,+1,+2,+3,+3,+2,+1,+0,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,
  +1,+3,+5,+6,+6,+5,+3,+1,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,
  +3,+5,+6,+8,+8,+6,+5,+3,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,
  +4,+5,+8,10,10,+8,+5,+4,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,
  +4,+5,+8,10,10,+8,+5,+4,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,
  +3,+5,+6,+8,+8,+6,+5,+3,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,
  +1,+3,+5,+6,+6,+5,+3,+1,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,
  +0,+1,+2,+3,+3,+2,+1,+0,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD
};

table_t king_position = {
  16,20,10,+3,10,+5,22,15,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,
  +6,+9,+3,+2,+2,+3,13,10,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,
  +2,+2,+1,+1,+1,+1,+2,+2,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,
  +0,+0,+0,+0,+0,+0,+0,+0,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,
  +0,+0,+0,+0,+0,+0,+0,+0,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,
  +2,+2,+1,+1,+1,+1,+2,+2,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,
  +6,+9,+3,+2,+2,+3,13,10,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,
  16,20,10,+3,10,+5,22,15,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD
};

table_t king_bnb_position = {
  +0,+1,+2,+3,+4,+5,+6,+7,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,
  +1,+2,+3,+4,+5,+6,+6,+6,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,
  +2,+3,+4,+8,+8,+6,+6,+5,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,
  +3,+4,+6,+8,+8,+8,+5,+4,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,
  +4,+5,+8,+8,+8,+6,+4,+3,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,
  +5,+6,+6,+5,+5,+4,+3,+2,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,
  +6,+6,+6,+5,+4,+3,+2,+1,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,
  +7,+6,+5,+4,+3,+2,+1,+0,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD
};

table_t king_bnw_position = {
  +7,+6,+5,+4,+3,+2,+1,+0,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,
  +6,+6,+6,+5,+4,+3,+2,+1,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,
  +5,+6,+6,+5,+5,+4,+3,+2,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,
  +4,+5,+8,+8,+8,+6,+4,+3,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,
  +3,+4,+6,+8,+8,+8,+5,+4,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,
  +2,+3,+4,+8,+8,+6,+6,+5,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,
  +1,+2,+3,+4,+5,+6,+6,+6,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD,
  +0,+1,+2,+3,+4,+5,+6,+7,BAD,BAD,BAD,BAD,BAD,BAD,BAD,BAD
};

/* bonus for the number of squares available to the sides of a rook 
   (not for rooks on half-open or open files) 
 */
int rook_side_to_side_bonus[8] = { -20, -12, -2, -1, 0, 1, 2, 3};

material_distribution_t mat_dist[MAT_DIST_MAX] = {
  /* 0 : no pieces at all */
  { 0, 1 }, 
  /* 1 : N */
  { KNIGHTVALUE, 1 },
  /* 2 : NN */
  { (KNIGHTVALUE * 2), 1 },
  /* 3: NNN */
  { (KNIGHTVALUE * 3), 1 },
  /* 4: B */
  { BISHOPVALUE, 1 },
  /* 5: BN */
  { BISHOPVALUE + KNIGHTVALUE, 1 },
  /* 6: BNN */
  { BISHOPVALUE + KNIGHTVALUE * 2, 1 },
  /* 7: BNNN */
  { BISHOPVALUE + KNIGHTVALUE * 3, 1 },
  /* 8: BB */
  { BISHOPVALUE * 2, 1 },
  /* 9: BBN */
  { BISHOPVALUE * 2 + KNIGHTVALUE, 1 },
  /* 10: BBNN */
  { BISHOPVALUE * 2 + KNIGHTVALUE * 2, 1 },
  /* 11: BBNNN */
  { BISHOPVALUE * 2 + KNIGHTVALUE * 3, 1 },
  /* 12: BBB */
  { BISHOPVALUE * 3 + KNIGHTVALUE * 0, 1 },
  /* 13: BBBN */
  { BISHOPVALUE * 3 + KNIGHTVALUE * 1, 1 },
  /* 14: BBBNN */
  { BISHOPVALUE * 3 + KNIGHTVALUE * 2, 1 },
  /* 15: BBBNNN */
  { BISHOPVALUE * 3 + KNIGHTVALUE * 3, 1 },
  /* 16 R */
  { ROOKVALUE, 1 }, 
  /* 17 : RN */
  { ROOKVALUE + KNIGHTVALUE, 1 },
  /* 18 : RNN */
  { ROOKVALUE + KNIGHTVALUE * 2, 1 },
  /* 19: RNNN */
  { ROOKVALUE + KNIGHTVALUE * 3, 1 },
  /* 20: RB */
  { ROOKVALUE + BISHOPVALUE, 1 },
  /* 21: RBN */
  { ROOKVALUE + BISHOPVALUE + KNIGHTVALUE, 1 },
  /* 22: RBNN */
  { ROOKVALUE + BISHOPVALUE + KNIGHTVALUE * 2, 1 },
  /* 23: RBNNN */
  { ROOKVALUE + BISHOPVALUE + KNIGHTVALUE * 3, 1 },
  /* 24: RBB */
  { ROOKVALUE + BISHOPVALUE * 2, 1 },
  /* 25: RBBN */
  { ROOKVALUE + BISHOPVALUE * 2 + KNIGHTVALUE, 1 },
  /* 26: RBBNN */
  { ROOKVALUE + BISHOPVALUE * 2 + KNIGHTVALUE * 2, 1 },
  /* 27: RBBNNN */
  { ROOKVALUE + BISHOPVALUE * 2 + KNIGHTVALUE * 3, 1 },
  /* 28: RBBB */
  { ROOKVALUE + BISHOPVALUE * 3 + KNIGHTVALUE * 0, 1 },
  /* 29: RBBBN */
  { ROOKVALUE + BISHOPVALUE * 3 + KNIGHTVALUE * 1, 1 },
  /* 30: RBBBNN */
  { ROOKVALUE + BISHOPVALUE * 3 + KNIGHTVALUE * 2, 1 },
  /* 31: RBBBNNN */
  { ROOKVALUE + BISHOPVALUE * 3 + KNIGHTVALUE * 3, 1 },
  /* 32: RR */
  { ROOKVALUE * 2, 1 }, 
  /* 33: RRN */
  { ROOKVALUE * 2 + KNIGHTVALUE, 1 },
  /* 34: RRNN */
  { ROOKVALUE * 2 + KNIGHTVALUE * 2, 1 },
  /* 35: RRNNN */
  { ROOKVALUE * 2 + KNIGHTVALUE * 3, 1 },
  /* 36: RRB */
  { ROOKVALUE * 2 + BISHOPVALUE, 1 },
  /* 37: RRBN */
  { ROOKVALUE * 2 + BISHOPVALUE + KNIGHTVALUE, 1 },
  /* 38: RRBNN */
  { ROOKVALUE * 2 + BISHOPVALUE + KNIGHTVALUE * 2, 1 },
  /* 39: RRBNNN */
  { ROOKVALUE * 2 + BISHOPVALUE + KNIGHTVALUE * 3, 1 },
  /* 40: RRBB */
  { ROOKVALUE * 2 + BISHOPVALUE * 2, 1 },
  /* 41: RRBBN */
  { ROOKVALUE * 2 + BISHOPVALUE * 2 + KNIGHTVALUE, 1 },
  /* 42: RRBBNN */
  { ROOKVALUE * 2 + BISHOPVALUE * 2 + KNIGHTVALUE * 2, 1 },
  /* 43: RRBBNNN */
  { ROOKVALUE * 2 + BISHOPVALUE * 2 + KNIGHTVALUE * 3, 1 },
  /* 44: RRBBB */
  { ROOKVALUE * 2 + BISHOPVALUE * 3 + KNIGHTVALUE * 0, 1 },
  /* 45: RRBBBN */
  { ROOKVALUE * 2 + BISHOPVALUE * 3 + KNIGHTVALUE * 1, 1 },
  /* 46: RRBBBNN */
  { ROOKVALUE * 2 + BISHOPVALUE * 3 + KNIGHTVALUE * 2, 1 },
  /* 47: RRBBBNNN */
  { ROOKVALUE * 2 + BISHOPVALUE * 3 + KNIGHTVALUE * 3, 1 },
  /* 16 pathological cases with 3 rooks */
  /* 48: RRR */
  { ROOKVALUE * 3, 1 }, 
  /* 49: RRRN */
  { ROOKVALUE * 3 + KNIGHTVALUE, 1 },
  /* 50: RRRNN */
  { ROOKVALUE * 3 + KNIGHTVALUE * 2, 1 },
  /* 51: RRRNNN */
  { ROOKVALUE * 3 + KNIGHTVALUE * 3, 1 },
  /* 52: RRRB */
  { ROOKVALUE * 3 + BISHOPVALUE, 1 },
  /* 53: RRRBN */
  { ROOKVALUE * 3 + BISHOPVALUE + KNIGHTVALUE, 1 },
  /* 54: RRRBNN */
  { ROOKVALUE * 3 + BISHOPVALUE + KNIGHTVALUE * 2, 1 },
  /* 55: RRRBNNN */
  { ROOKVALUE * 3 + BISHOPVALUE + KNIGHTVALUE * 3, 1 },
  /* 56: RRRBB */
  { ROOKVALUE * 3 + BISHOPVALUE * 2, 1 },
  /* 57: RRRBBN */
  { ROOKVALUE * 3 + BISHOPVALUE * 2 + KNIGHTVALUE, 1 },
  /* 58: RRRBBNN */
  { ROOKVALUE * 3 + BISHOPVALUE * 2 + KNIGHTVALUE * 2, 1 },
  /* 59: RRRBBNNN */
  { ROOKVALUE * 3 + BISHOPVALUE * 2 + KNIGHTVALUE * 3, 1 },
  /* 60: RRRBBB */
  { ROOKVALUE * 3 + BISHOPVALUE * 3 + KNIGHTVALUE * 0, 1 },
  /* 61: RRRBBBN */
  { ROOKVALUE * 3 + BISHOPVALUE * 3 + KNIGHTVALUE * 1, 1 },
  /* 62: RRRBBBNN */
  { ROOKVALUE * 3 + BISHOPVALUE * 3 + KNIGHTVALUE * 2, 1 },
  /* 63: RRRBBBNNN */
  { ROOKVALUE * 3 + BISHOPVALUE * 3 + KNIGHTVALUE * 3, 1 },
  /* 64: Q*/
  { QUEENVALUE, 1 }, 
  /* 65 : QN */
  { QUEENVALUE + KNIGHTVALUE, 1 },
  /* 66 : QNN */
  { QUEENVALUE + KNIGHTVALUE * 2, 1 },
  /* 67: QNNN */
  { QUEENVALUE + KNIGHTVALUE * 3, 1 },
  /* 68: QB */
  { QUEENVALUE + BISHOPVALUE, 1 },
  /* 69: QBN */
  { QUEENVALUE + BISHOPVALUE + KNIGHTVALUE, 1 },
  /* 70: QBNN */
  { QUEENVALUE + BISHOPVALUE + KNIGHTVALUE * 2, 1 },
  /* 71: QBNNN */
  { QUEENVALUE + BISHOPVALUE + KNIGHTVALUE * 3, 1 },
  /* 72: QBB */
  { QUEENVALUE + BISHOPVALUE * 2, 1 },
  /* 73: QBBN */
  { QUEENVALUE + BISHOPVALUE * 2 + KNIGHTVALUE, 1 },
  /* 74: QBBNN */
  { QUEENVALUE + BISHOPVALUE * 2 + KNIGHTVALUE * 2, 1 },
  /* 75: QBBNNN */
  { QUEENVALUE + BISHOPVALUE * 2 + KNIGHTVALUE * 3, 1 },
  /* 76: QBBB */
  { QUEENVALUE + BISHOPVALUE * 3 + KNIGHTVALUE * 0, 1 },
  /* 77: QBBBN */
  { QUEENVALUE + BISHOPVALUE * 3 + KNIGHTVALUE * 1, 1 },
  /* 78: QBBBNN */
  { QUEENVALUE + BISHOPVALUE * 3 + KNIGHTVALUE * 2, 1 },
  /* 79: QBBBNNN */
  { QUEENVALUE + BISHOPVALUE * 3 + KNIGHTVALUE * 3, 1 },
  /* 80 QR */
  { QUEENVALUE + ROOKVALUE, 1 }, 
  /* 81 : QRN */
  { QUEENVALUE + ROOKVALUE + KNIGHTVALUE, 1 },
  /* 82 : QRNN */
  { QUEENVALUE + ROOKVALUE + KNIGHTVALUE * 2, 1 },
  /* 83: QRNNN */
  { QUEENVALUE + ROOKVALUE + KNIGHTVALUE * 3, 1 },
  /* 84: QRB */
  { QUEENVALUE + ROOKVALUE + BISHOPVALUE, 1 },
  /* 85: QRBN */
  { QUEENVALUE + ROOKVALUE + BISHOPVALUE + KNIGHTVALUE, 1 },
  /* 86: QRBNN */
  { QUEENVALUE + ROOKVALUE + BISHOPVALUE + KNIGHTVALUE * 2, 1 },
  /* 87: QRBNNN */
  { QUEENVALUE + ROOKVALUE + BISHOPVALUE + KNIGHTVALUE * 3, 1 },
  /* 88: QRBB */
  { QUEENVALUE + ROOKVALUE + BISHOPVALUE * 2, 1 },
  /* 89: QRBBN */
  { QUEENVALUE + ROOKVALUE + BISHOPVALUE * 2 + KNIGHTVALUE, 1 },
  /* 90: QRBBNN */
  { QUEENVALUE + ROOKVALUE + BISHOPVALUE * 2 + KNIGHTVALUE * 2, 1 },
  /* 91: QRBBNNN */
  { QUEENVALUE + ROOKVALUE + BISHOPVALUE * 2 + KNIGHTVALUE * 3, 1 },
  /* 92: QRBBB */
  { QUEENVALUE + ROOKVALUE + BISHOPVALUE * 3 + KNIGHTVALUE * 0, 1 },
  /* 93: QRBBBN */
  { QUEENVALUE + ROOKVALUE + BISHOPVALUE * 3 + KNIGHTVALUE * 1, 1 },
  /* 94: QRBBBNN */
  { QUEENVALUE + ROOKVALUE + BISHOPVALUE * 3 + KNIGHTVALUE * 2, 1 },
  /* 95: QRBBBNNN */
  { QUEENVALUE + ROOKVALUE + BISHOPVALUE * 3 + KNIGHTVALUE * 3, 1 },
  /* 96: QRR */
  { QUEENVALUE + ROOKVALUE * 2, 1 }, 
  /* 97: QRRN */
  { QUEENVALUE + ROOKVALUE * 2 + KNIGHTVALUE, 1 },
  /* 98: QRRNN */
  { QUEENVALUE + ROOKVALUE * 2 + KNIGHTVALUE * 2, 1 },
  /* 99: QRRNNN */
  { QUEENVALUE + ROOKVALUE * 2 + KNIGHTVALUE * 3, 1 },
  /* 100: QRRB */
  { QUEENVALUE + ROOKVALUE * 2 + BISHOPVALUE, 1 },
  /* 101: QRRBN */
  { QUEENVALUE + ROOKVALUE * 2 + BISHOPVALUE + KNIGHTVALUE, 1 },
  /* 102: QRRBNN */
  { QUEENVALUE + ROOKVALUE * 2 + BISHOPVALUE + KNIGHTVALUE * 2, 1 },
  /* 103: QRRBNNN */
  { QUEENVALUE + ROOKVALUE * 2 + BISHOPVALUE + KNIGHTVALUE * 3, 1 },
  /* 104: QRRBB */
  { QUEENVALUE + ROOKVALUE * 2 + BISHOPVALUE * 2, 1 },
  /* 105: QRRBBN */
  { QUEENVALUE + ROOKVALUE * 2 + BISHOPVALUE * 2 + KNIGHTVALUE, 1 },
  /* 106: QRRBBNN */
  { QUEENVALUE + ROOKVALUE * 2 + BISHOPVALUE * 2 + KNIGHTVALUE * 2, 1 },
  /* 107: QRRBBNNN */
  { QUEENVALUE + ROOKVALUE * 2 + BISHOPVALUE * 2 + KNIGHTVALUE * 3, 1 },
  /* 108: QRRBBB */
  { QUEENVALUE + ROOKVALUE * 2 + BISHOPVALUE * 3 + KNIGHTVALUE * 0, 1 },
  /* 109: QRRBBBN */
  { QUEENVALUE + ROOKVALUE * 2 + BISHOPVALUE * 3 + KNIGHTVALUE * 1, 1 },
  /* 110: QRRBBBNN */
  { QUEENVALUE + ROOKVALUE * 2 + BISHOPVALUE * 3 + KNIGHTVALUE * 2, 1 },
  /* 111: QRRBBBNNN */
  { QUEENVALUE + ROOKVALUE * 2 + BISHOPVALUE * 3 + KNIGHTVALUE * 3, 1 },
  /* 16 pathological cases with 3 rooks */
  /* 112: QRRR */
  { QUEENVALUE + ROOKVALUE * 3, 1 }, 
  /* 113: QRRRN */
  { QUEENVALUE + ROOKVALUE * 3 + KNIGHTVALUE, 1 },
  /* 114: QRRRNN */
  { QUEENVALUE + ROOKVALUE * 3 + KNIGHTVALUE * 2, 1 },
  /* 115: QRRRNNN */
  { QUEENVALUE + ROOKVALUE * 3 + KNIGHTVALUE * 3, 1 },
  /* 116: QRRRB */
  { QUEENVALUE + ROOKVALUE * 3 + BISHOPVALUE, 1 },
  /* 117: QRRRBN */
  { QUEENVALUE + ROOKVALUE * 3 + BISHOPVALUE + KNIGHTVALUE, 1 },
  /* 118: QRRRBNN */
  { QUEENVALUE + ROOKVALUE * 3 + BISHOPVALUE + KNIGHTVALUE * 2, 1 },
  /* 119: QRRRBNNN */
  { QUEENVALUE + ROOKVALUE * 3 + BISHOPVALUE + KNIGHTVALUE * 3, 1 },
  /* 120: QRRRBB */
  { QUEENVALUE + ROOKVALUE * 3 + BISHOPVALUE * 2, 1 },
  /* 121: QRRRBBN */
  { QUEENVALUE + ROOKVALUE * 3 + BISHOPVALUE * 2 + KNIGHTVALUE, 1 },
  /* 122: QRRRBBNN */
  { QUEENVALUE + ROOKVALUE * 3 + BISHOPVALUE * 2 + KNIGHTVALUE * 2, 1 },
  /* 123: QRRRBBNNN */
  { QUEENVALUE + ROOKVALUE * 3 + BISHOPVALUE * 2 + KNIGHTVALUE * 3, 1 },
  /* 124: QRRRBBB */
  { QUEENVALUE + ROOKVALUE * 3 + BISHOPVALUE * 3 + KNIGHTVALUE * 0, 1 },
  /* 125: QRRRBBBN */
  { QUEENVALUE + ROOKVALUE * 3 + BISHOPVALUE * 3 + KNIGHTVALUE * 1, 1 },
  /* 126: QRRRBBBNN */
  { QUEENVALUE + ROOKVALUE * 3 + BISHOPVALUE * 3 + KNIGHTVALUE * 2, 1 },
  /* 127: QRRRBBBNNN */
  { QUEENVALUE + ROOKVALUE * 3 + BISHOPVALUE * 3 + KNIGHTVALUE * 3, 1 },
};

/*
 * Calculate piece material score for one side from given 
 * 16-bit signature.
 * 
 * Returns: material value in centipawns. 
 *
 * This function is used for uncommon material configurations
 * (e.g., this side has either more than 1 queen, 3 rooks, 3 bishops 
 * or 3 knights). Input is are the lower 2 bytes from the material
 * signature in move_flags.
 */

unsigned int
piece_score_from_sig(unsigned int sig)
{
  unsigned int score;

  score = (sig & 0x000f) * KNIGHTVALUE;
  score += ((sig & 0x00f0) >> 4) * BISHOPVALUE;
  score += ((sig & 0x0f00) >> 8) * ROOKVALUE;
  score += ((sig & 0xf000) >> 12) * QUEENVALUE;

  return score;
}

/*
 * convienience function to extract sum of piece values from
 * move_flags.[w|b]_mat.
 * This is just to ease the transition from the GET_PIECE_MATERIAL
 * macro. 
 *
 * Where it really matters (evaluation), the short signature can
 * be used to get the remaining info from mat_dist_mat table.h
 */

unsigned int 
get_piece_material(unsigned int sig)
{
  int sig7;
  
  sig &= 0x0000ffff; /* XXX this is currently redundant */
  
  sig7 = make_mat_dist_index(sig);
  if(sig7 == MAT_DIST_INVALID_INDEX)
    return piece_score_from_sig(sig);

  return get_mat_dist_mat(sig7);
}

