/**
 * @file ndn_coding.c
 * @brief Support for scanning and parsing ndnb-encoded data.
 *
 * Part of the NDNx C Library.
 *
 * Portions Copyright (C) 2013 Regents of the University of California.
 *
 * Based on the CCNx C Library by PARC.
 * Copyright (C) 2008, 2009 Palo Alto Research Center, Inc.
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 2.1
 * as published by the Free Software Foundation.
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details. You should have received
 * a copy of the GNU Lesser General Public License along with this library;
 * if not, write to the Free Software Foundation, Inc., 51 Franklin Street,
 * Fifth Floor, Boston, MA 02110-1301 USA.
 */
#include <ndn/coding.h>

/**
 * This macro documents what's happening in the state machine by
 * hinting at the XML syntax would be emitted in a re-encoder.
 * But it actually does nothing.
 */
#define XML(goop) ((void)0)

/**
 * Decodes ndnb decoded data
 *
 * 解码ndnb数据
 *
 * @param d holds the current state of the decoder.
 * @param p points to a new block of ndnb data to feed to the decoder.
 * @param n is the size of the input, in bytes.
 * @returns the number of bytes consumed.
 *
 * 参数：
 * d： 当前解码器的状态
 * p： 传入的ndnb数据的指针
 * n： 传入数据的大小，单位字节
 * 返回：被消费的字节数
 *
 * The client should ensure that the decoder is initialized to all zero
 * before the first call.  In the default mode, the decoder will return
 * only when it runs out of data, encounters an error, or reaches the end
 * of the element that it started at.  This is a good way to pull
 * ndnb-encoded objects from a byte stream.
 *
 * 客户端要保证在第一次调用之前解码器要被初始化为全0
 *
 * By setting the NDN_DSTATE_PAUSE bit is set in the decoder state, the
 * decoder will additionally return just after recognizing each token.
 * In this instance, use NDN_GET_TT_FROM_DSTATE() to extract
 * the token type from the decoder state;
 * NDN_CLOSE will be reported as NDN_NO_TOKEN.
 *
 * 在解码器状态中设置NDN_DSTATE_PAUSE位会让解码器在识别每一个token后立即返回。
 * 在这个实体中，使用NDN_GET_TT_FROM_DSTATE()来从解码器状态中获取token类型。
 *
 * The pause bit persists, so the end test should take that into account
 * by using the NDN_FINAL_DSTATE() macro instead of testing for state 0.
 *
 * 停止位一直存在，所以结束测试要使用NDN_FINAL_DSTATE()而不是测试状态为0.
 *
 * 一旦检测到错误，将不会再处理输入。
 *
 * Once an error state is entered, no addition input is processed.
 *
 * @see ndn_buf_decoder_start(), ndn_buf_advance(), ndn_buf_check_close()
 */

 // 状态d作为返回
ssize_t
ndn_skeleton_decode(struct ndn_skeleton_decoder *d,
                    const unsigned char *p, size_t n)
{
    enum ndn_decoder_state state = d->state;
    int tagstate = 0;
    size_t numval = d->numval;
    ssize_t i = 0;
    unsigned char c;
    size_t chunk;
    int pause = 0;
    if (d->state >= 0) {
        pause = d->state & NDN_DSTATE_PAUSE;
        tagstate = (d->state >> 8) & 3;
        state = d->state & 0xFF;
    }
    while (i < n) {
        switch (state) {
            case NDN_DSTATE_INITIAL:
            case NDN_DSTATE_NEWTOKEN: /* start new thing */
                d->token_index = i + d->index;
                if (tagstate > 1 && tagstate-- == 2) {
                    XML("\""); /* close off the attribute value */
                }
                if (p[i] == NDN_CLOSE) {
                    i++;
                    if (d->nest <= 0 || tagstate > 1) {
                        state = NDN_DSTATE_ERR_NEST;
                        break;
                    }
                    if (tagstate == 1) {
                        tagstate = 0;
                        XML("/>");
                    }
                    else {
                        XML("</%s>");
                    }
                    d->nest -= 1;
                    if (d->nest == 0) {
                        state = NDN_DSTATE_INITIAL;
                        n = i;
                    }
                    if (pause) {
                        state |= (((int)NDN_NO_TOKEN) << 16);
                        n = i;
                    }
                    break;
                }
                numval = 0;
                state = NDN_DSTATE_NUMVAL;
                /* FALLTHRU */
            case NDN_DSTATE_NUMVAL: /* parsing numval */
                c = p[i++];
                if ((c & NDN_TT_HBIT) == NDN_CLOSE) {
                    if (numval > ((~(size_t)0U) >> (7 + NDN_TT_BITS)))
                        state = NDN_DSTATE_ERR_OVERFLOW;
                    numval = (numval << 7) + (c & 127);
                }
                else {
                    numval = (numval << (7-NDN_TT_BITS)) +
                             ((c >> NDN_TT_BITS) & NDN_MAX_TINY);
                    c &= NDN_TT_MASK;
                    switch (c) {
                        case NDN_EXT:
                            if (tagstate == 1) {
                                tagstate = 0;
                                XML(">");
                            }
                            d->nest += 1;
                            d->element_index = d->token_index;
                            state = NDN_DSTATE_NEWTOKEN;
                            break;
                        case NDN_DTAG:
                            if (tagstate == 1) {
                                tagstate = 0;
                                XML(">");
                            }
                            d->nest += 1;
                            d->element_index = d->token_index;
                            XML("<%s");
                            tagstate = 1;
                            state = NDN_DSTATE_NEWTOKEN;
                            break;
                        case NDN_BLOB:
                            if (tagstate == 1) {
                                tagstate = 0;
                                XML(" ndnbencoding=\"base64Binary\">");
                            }
                            state = NDN_DSTATE_BLOB;
                            if (numval == 0)
                                state = NDN_DSTATE_NEWTOKEN;
                            break;
                        case NDN_UDATA:
                            if (tagstate == 1) {
                                tagstate = 0;
                                XML(">");
                            }
                            state = NDN_DSTATE_UDATA;
                            if (numval == 0)
                                state = NDN_DSTATE_NEWTOKEN;
                            break;
                        case NDN_DATTR:
                            if (tagstate != 1) {
                                state = NDN_DSTATE_ERR_ATTR;
                                break;
                            }
                            tagstate = 3;
                            state = NDN_DSTATE_NEWTOKEN;
                            break;
                        case NDN_ATTR:
                            if (tagstate != 1) {
                                state = NDN_DSTATE_ERR_ATTR;
                                break;
                            }
                            numval += 1; /* encoded as length-1 */
                            state = NDN_DSTATE_ATTRNAME;
                            break;
                        case NDN_TAG:
                            if (tagstate == 1) {
                                tagstate = 0;
                                XML(">");
                            }
                            numval += 1; /* encoded as length-1 */
                            d->nest += 1;
                            d->element_index = d->token_index;
                            state = NDN_DSTATE_TAGNAME;
                            break;
                        default:
                            state = NDN_DSTATE_ERR_CODING;
                    }
                    if (pause) {
                        state |= (c << 16);
                        n = i;
                    }
                }
                break;
            case NDN_DSTATE_TAGNAME: /* parsing tag name */
                chunk = n - i;
                if (chunk > numval)
                    chunk = numval;
                if (chunk == 0) {
                    state = NDN_DSTATE_ERR_BUG;
                    break;
                }
                numval -= chunk;
                i += chunk;
                if (numval == 0) {
                    if (d->nest == 0) {
                        state = NDN_DSTATE_ERR_NEST;
                        break;
                    }
                    XML("<%s");
                    tagstate = 1;
                    state = NDN_DSTATE_NEWTOKEN;
                }
                break;
            case NDN_DSTATE_ATTRNAME: /* parsing attribute name */
                chunk = n - i;
                if (chunk > numval)
                    chunk = numval;
                if (chunk == 0) {
                    state = NDN_DSTATE_ERR_BUG;
                    break;
                }
                numval -= chunk;
                i += chunk;
                if (numval == 0) {
                    if (d->nest == 0) {
                        state = NDN_DSTATE_ERR_ATTR;
                        break;
                    }
                    XML(" %s=\"");
                    tagstate = 3;
                    state = NDN_DSTATE_NEWTOKEN;
                }
                break;
            case NDN_DSTATE_UDATA: /* utf-8 data */
            case NDN_DSTATE_BLOB: /* BLOB */
                chunk = n - i;
                if (chunk > numval)
                    chunk = numval;
                if (chunk == 0) {
                    state = NDN_DSTATE_ERR_BUG;
                    break;
                }
                numval -= chunk;
                i += chunk;
                if (numval == 0)
                    state = NDN_DSTATE_NEWTOKEN;
                break;
            default:
                n = i;
        }
    }
    if (state < 0)
        tagstate = pause = 0;
    d->state = state | pause | (tagstate << 8);
    d->numval = numval;
    d->index += i;
    return(i);
}
