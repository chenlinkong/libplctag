/***************************************************************************
 *   Copyright (C) 2015 by OmanTek                                         *
 *   Author Kyle Hayes  kylehayes@omantek.com                              *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU Library General Public License as       *
 *   published by the Free Software Foundation; either version 2 of the    *
 *   License, or (at your option) any later version.                       *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU Library General Public License for more details.                  *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this program; if not, write to the                 *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

/**************************************************************************
 * CHANGE LOG                                                             *
 *                                                                        *
 * 2015-09-12  KRH - Created file.                                        *
 *                                                                        *
 **************************************************************************/


#include <ab/ab_common.h>
#include <ab/request.h>
#include <platform.h>
#include <ab/session.h>
#include <util/debug.h>
#include <util/refcount.h>


/*
 * request_create
 *
 * This does not do much for now other than allocate memory.  In the future
 * it may be desired to keep a pool of request buffers instead.  This shim
 * is here so that such a change can be done without major code changes
 * elsewhere.
 */
int request_create(ab_request_p* req)
{
    int rc = PLCTAG_STATUS_OK;
    ab_request_p res;

    res = (ab_request_p)mem_alloc(sizeof(struct ab_request_t));

    if (!res) {
        *req = NULL;
        rc = PLCTAG_ERR_NO_MEM;
    } else {
        res->rc = refcount_init(1, global_session_mut, res, request_destroy);
        *req = res;
    }

    return rc;
}

/*
 * request_add_unsafe
 *
 * You must hold the mutex before calling this!
 */
int request_add_unsafe(ab_session_p sess, ab_request_p req)
{
    int rc = PLCTAG_STATUS_OK;
    ab_request_p cur, prev;

    pdebug(DEBUG_DETAIL, "Starting.");

    if(!sess) {
        pdebug(DEBUG_WARN, "Session is null!");
        return PLCTAG_ERR_NULL_PTR;
    }

    /* make sure the request points to the session */
    req->session = sess;

    /* we add the request to the end of the list. */
    cur = sess->requests;
    prev = NULL;

    while (cur) {
        prev = cur;
        cur = cur->next;
    }

    if (!prev) {
        sess->requests = req;
    } else {
        prev->next = req;
    }

    /* update the session's refcount as we point to it. */
    session_acquire_unsafe(sess);

    pdebug(DEBUG_DETAIL, "Done.");

    return rc;
}

/*
 * request_add
 *
 * This is a thread-safe version of the above routine.
 */
int request_add(ab_session_p sess, ab_request_p req)
{
    int rc = PLCTAG_STATUS_OK;

    pdebug(DEBUG_DETAIL, "Starting. sess=%p, req=%p", sess, req);

    critical_block(global_session_mut) {
        rc = request_add_unsafe(sess, req);
    }

    pdebug(DEBUG_DETAIL, "Done.");

    return rc;
}

/*
 * request_remove_unsafe
 *
 * You must hold the mutex before calling this!
 */
int request_remove_unsafe(ab_session_p sess, ab_request_p req)
{
    int rc = PLCTAG_STATUS_OK;
    ab_request_p cur, prev;

    pdebug(DEBUG_DETAIL, "Starting.");

    if(sess == NULL || req == NULL) {
        return rc;
    }

    /* find the request and remove it from the list. */
    cur = sess->requests;
    prev = NULL;

    while (cur && cur != req) {
        prev = cur;
        cur = cur->next;
    }

    if (cur == req) {
        if (!prev) {
            sess->requests = cur->next;
        } else {
            prev->next = cur->next;
        }
    } /* else not found */

    req->next = NULL;
    req->session = NULL;

    /* release the session refcount */
    session_release_unsafe(sess);

    pdebug(DEBUG_DETAIL, "Done.");

    return rc;
}

/*
 * request_remove
 *
 * This is a thread-safe version of the above routine.
 */
int request_remove(ab_session_p sess, ab_request_p req)
{
    int rc = PLCTAG_STATUS_OK;

    pdebug(DEBUG_DETAIL, "Starting.");

    if(sess == NULL || req == NULL) {
        return rc;
    }

    critical_block(global_session_mut) {
        rc = request_remove_unsafe(sess, req);
    }

    pdebug(DEBUG_DETAIL, "Done.");

    return rc;
}

/*
 * request_destroy
 *
 * The request must be removed from any lists before this!
 */
int request_destroy_unsafe(ab_request_p* req_pp)
{
    ab_request_p r;
    /* int debug; */

    pdebug(DEBUG_DETAIL, "Starting.");

    if(req_pp && *req_pp) {
        r = *req_pp;

        /* debug = r->debug; */

        request_remove_unsafe(r->session, r);
        mem_free(r);
        *req_pp = NULL;
    }

    pdebug(DEBUG_DETAIL, "Done.");

    return PLCTAG_STATUS_OK;
}


int request_destroy(ab_request_p* req_pp)
{
    critical_block(global_session_mut) {
        request_destroy_unsafe(req_pp);
    }

    return PLCTAG_STATUS_OK;
}
