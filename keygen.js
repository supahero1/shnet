let a = '';
let b = '_abcd_efgh_ijkl_mnop_qrst_uvwx_yzAB_CDEF_GHIJ_KLMN_OPQR_STUV_WXYZ_0123_4567_89';
for(let i = 0; i < 32; ++i) {
  a += b[(Math.random() * b.length) | 0];
}
a;