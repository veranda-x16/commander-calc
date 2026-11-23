/* tools/veranda_keys.h -- the keys of tools/gate_veranda.sh's session,
 * as grid.c's codes (the X16 ROM's ISO codes). Played on Veranda through
 * the ROM's key queue (src/tools/veranda_play.c) and on the host straight
 * into grid_key() (tools/veranda_golden.c).
 *
 *   12 RETURN 30 RETURN =SUM(A1:A2)*2 RETURN   A1, A2, A3
 *   up up up  5 RETURN                         A1 = 5: A3 is 70
 *   PgDn PgUp up right                          (scroll, back: B1)
 */
static const unsigned char play_keys[] = {
    '1', '2', 0x0D, '3', '0', 0x0D,
    '=', 'S', 'U', 'M', '(', 'A', '1', ':', 'A', '2', ')', '*', '2', 0x0D,
    0x91, 0x91, 0x91, '5', 0x0D,
    0x02, 0x82, 0x91, 0x1D,
};
#define PLAY_N ((unsigned char)sizeof play_keys)
