#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "event.h"
#include "atomic.h"
#include "mem.h"
#include "threadpool.h"
#include "str.h"

#define MAX_STREP_SIZE  4096

typedef struct evm_def_s evm_def_t;
typedef struct evm_event_grp_s evm_event_grp_t;

/**
 * @brief An event listener.
 */
struct evm_listener_s
{
    evm_def_t *def; /*!< The event type definition being listened to. */
    evm_event_cb cb; /*!< Callback for the event. */
    evm_listener_destroy_cb destroycb; /*!< Callback when the listener is about to be destroyed. */
    void *user; /*!< User defined data for the callback. */
    evm_listener_t *next; /*!< Next listener. */
};

/**
 * @brief An event type defintion.
 */
struct evm_def_s
{
    evm_eid eid; /*!< The event type identifier. */
    evm_event_formatter formatter; /*!< The event string formatter function. */
    evm_ctx_t *ctx; /*!< The event context to which it was defined. */
    evm_listener_t *listeners; /*!< List of listeners of this event type. */
    evm_def_t *next; /*!< Next event defintion. */
};

/**
 * @brief An event.
 */
struct evm_event_s
{
    evm_def_t *def; /*!< The event defintion for this event. */
    void *data; /*!< Data associated with the event. */
    bool dispatched; /*!< Whether or not this event was dispatched. */
    evm_event_grp_t *grp; /*!< Event group this event belongs to. */
    evm_event_destroy_cb destroycb; /*!< Event destroy callback function. */
    void *user; /*!< Callback user-defined data. */
    char *strep; /*!< String representation of the event data. */
    struct evm_event_s *next; /*!< Next event. */
};

/**
 * @brief An event group.
 *
 * A group of events triggered by a common
 * ancestor event.
 */
struct evm_event_grp_s
{
    evm_session_t *session; /*!< The session for the group. */
    uint32_t depth; /*!< Current dependent depth. */
    evm_event_t *head; /*!< The event queue head. */
    evm_event_t *tail; /*!< The event queue tail. */
    evm_event_grp_t *next; /*!< Next event in the group. */
};

/**
 * @brief An event session.
 *
 * A session begins with an initial event trigger from
 * which subsequent dependent events may be generated.
 * The session determines whether or not the current
 * batch of dependent events are dispatched or not.
 */
struct evm_session_s
{
    evm_ctx_t *ctx; /*!< The context to which the session belongs. */
    evm_event_grp_t *head; /*!< Dependent event group queue head. */
    evm_event_grp_t *tail; /*!< Dependent event group queue tail. */
    evm_session_cb cb; /*!< Session callback function. */
    void *user; /*!< Callback user-defined data. */
    evm_session_t *next; /*!< Next session in the list. */
};

/**
 * @brief An event engine context.
 */
struct evm_ctx_s
{
    size_t wcnt; /*!< Number of worker threads. */
    tp_thread_t **workers; /*!< Session queue worker threads (must have at least one). */
    tp_thread_t *maintenance; /*!< Maintenance thread. */
    uint32_t mfreq; /*!< Maintenance frequency. */
    pthread_mutex_t mutex; /*!< Session queue consumer mutex. */
    pthread_cond_t condv; /*!< Session queue condition variable. */
    pthread_rwlock_t lock; /*!< Read-write lock for event definition synchronization. */
    int qspin; /*!< Atomic spin-lock for session queue synchronization. */
    evm_def_t *defs; /*!< List of event type definitions. */
    evm_session_t *head; /*!< Session queue head. */
    evm_session_t *tail; /*!< Session queue tail. */
};

evm_ctx_t *evm_gctx = NULL; /*!< Global event manager singleton. */


evm_def_t *evm_find_def(evm_ctx_t *ctx, evm_eid eid);
bool evm_dispatch_event(evm_event_t *event);

evm_session_t *evm_create_session(evm_ctx_t *ctx, evm_session_cb cb, void *user);
evm_def_t *evm_create_def(evm_ctx_t *ctx, evm_eid eid, evm_event_formatter formatter);
evm_listener_t *evm_create_listener(evm_def_t *def, evm_event_cb cb, evm_listener_destroy_cb destroycb, void *user);
evm_event_grp_t *evm_create_grp(evm_session_t *session, uint32_t depth);
evm_event_t *evm_create_event(evm_def_t *def, evm_event_grp_t *grp, void *data, evm_event_destroy_cb cb, void *user);

void evm_destroy_session(evm_session_t *session);
void evm_destroy_def(evm_def_t *def);
void evm_destroy_listener(evm_listener_t *listener);
void evm_destroy_grp(evm_event_grp_t *grp);
void evm_destroy_event(evm_event_t *event);

evm_session_t *evm_ctx_push_session(evm_ctx_t *ctx, evm_session_t *session);
evm_session_t *evm_ctx_pop_session(evm_ctx_t *ctx);
evm_session_t *evm_ctx_front_session(evm_ctx_t *ctx);
evm_session_t *evm_ctx_back_session(evm_ctx_t *ctx);

evm_event_grp_t *evm_session_push_grp(evm_session_t *session, evm_event_grp_t *grp);
evm_event_grp_t *evm_session_pop_grp(evm_session_t *session);
evm_event_grp_t *evm_session_front_grp(evm_session_t *session);
evm_event_grp_t *evm_session_back_grp(evm_session_t *session);

evm_event_t *evm_grp_push_event(evm_event_grp_t *grp, evm_event_t *event);
evm_event_t *evm_grp_pop_event(evm_event_grp_t *grp);
evm_event_t *evm_grp_front_event(evm_event_grp_t *grp);
evm_event_t *evm_grp_back_event(evm_event_grp_t *grp);

tp_code_t evm_worker(void *data);
tp_code_t evm_maintenance(void *data);

evm_eid evm_event_id(evm_event_t *event)
{
    evm_eid retval = 0;
    if(event != NULL)
    {
        retval = event->def->eid;
    }

    return retval;
}

void *evm_event_data(evm_event_t *event)
{
    void *retval = NULL;
    if(event != NULL)
    {
        retval = event->data;
    }

    return retval;
}

const char *evm_event_strep(evm_event_t *event)
{
    char *retval = NULL;
    if(event != NULL)
    {
        //
        // Generate the string representation if it has not already been done.
        //
        if(event->strep == NULL)
        {
            event->strep = ALLOC(MAX_STREP_SIZE);
            if(event->strep != NULL)
            {
                memset(event->strep, 0, MAX_STREP_SIZE);
                if(event->def->formatter != NULL)
                {
                    event->def->formatter(event, event->strep, MAX_STREP_SIZE);
                }
            }
        }

        //
        // Return the string representation.
        //
        retval = event->strep;
    }

    return retval;
}


evm_eid evm_generate_eid(char* event)
{
    return (event!=NULL)?strid(event):0;
}

evm_ctx_t *evm_singleton()
{
    if(evm_gctx == NULL)
    {
        evm_gctx = evm_initialize(2, 300);
    }

    return evm_gctx;
}

evm_ctx_t *evm_initialize(size_t nworkers, uint32_t mfreq)
{
    evm_ctx_t *retval = ALLOC(sizeof(*retval));
    if(retval != NULL)
    {
        memset(retval, 0, sizeof(*retval));

        //
        // Default maintenance frequency. Don't allow this to
        // be zero.
        //
        retval->mfreq = (mfreq == 0) ? 60 : mfreq;

        //
        // Initialize all the synchronization mechanisms and try to create
        // the maintenance thread.
        //
        if((pthread_mutex_init(&retval->mutex, NULL) == 0) &&
           (pthread_cond_init(&retval->condv, NULL) == 0) &&
           (pthread_rwlock_init(&retval->lock, NULL) == 0) &&
           ((retval->maintenance = tp_request_thread(0, evm_maintenance, retval)) != NULL))
        {
            //
            // Try to create the worker threads.
            //
            nworkers = (nworkers == 0) ? 1 : nworkers;
            retval->workers = ALLOC(nworkers * sizeof(*(retval->workers)));
            if(retval->workers != NULL)
            {
                memset(retval->workers, 0, nworkers * (sizeof(*(retval->workers))));
                //
                // Create the worker threads.
                //
                for(; retval->wcnt<nworkers; ++retval->wcnt)
                {
                    retval->workers[retval->wcnt] = tp_request_thread(10, evm_worker, retval);
                    if(retval->workers[retval->wcnt] == NULL)
                    {
                        break;
                    }
                }
            }

            //
            // Check if the worker threads were created.
            //
            if(retval->wcnt == 0)
            {
                pthread_mutex_destroy(&retval->mutex);
                pthread_cond_destroy(&retval->condv);
                pthread_rwlock_destroy(&retval->lock);
                FREE(retval->workers);
                FREE(retval);
                retval = NULL;
            }
        }
        else
        {
            pthread_mutex_destroy(&retval->mutex);
            pthread_cond_destroy(&retval->condv);
            pthread_rwlock_destroy(&retval->lock);
            FREE(retval);
            retval = NULL;
        }
    }

    return retval;
}

void evm_destroy(evm_ctx_t *ctx)
{
    //
    // Check if we are using the singleton.
    //
    if((ctx == NULL) && (evm_gctx != NULL))
    {
        ctx = evm_gctx;
        evm_gctx = NULL;
    }

    if(ctx != NULL)
    {
        //
        // Stop the worker threads.
        //
        for(size_t i = 0; i<ctx->wcnt; ++i)
        {
            tp_release_thread(ctx->workers[i]);
            tp_wait_for_thread(ctx->workers[i]);
        }

        //
        // Stop the maintainence thread.
        //
        tp_release_thread(ctx->maintenance);
        tp_wait_for_thread(ctx->maintenance);

        //
        // Release the synchronization resources.
        //
        pthread_mutex_destroy(&ctx->mutex);
        pthread_cond_destroy(&ctx->condv);
        pthread_rwlock_destroy(&ctx->lock);

        //
        // Release the sessions.
        //
        evm_session_t *curr = ctx->head;
        while(curr != NULL)
        {
            evm_session_t *tmp = curr->next;
            evm_destroy_session(curr);
            curr = tmp;
        }

        //
        // Release the event definitions.
        //
        evm_def_t *def = ctx->defs;
        while(def != NULL)
        {
            evm_def_t *tmp = def->next;
            evm_destroy_def(def);
            def = tmp;
        }

        //
        // Deallocate the memory.
        //
        FREE(ctx->workers);
        FREE(ctx);
    }
}

bool evm_register_type(evm_ctx_t *ctx, evm_eid eid, evm_event_formatter formatter)
{
    bool retval = false;

    //
    // Check if we are using the singleton.
    //
    if(ctx == NULL)
    {
        ctx = evm_singleton();
    }

    if(ctx != NULL)
    {
        //
        // Make sure the event definition does not already exist.
        //
        evm_def_t *def = evm_find_def(ctx, eid);
        if(def == NULL)
        {
            //
            // Create a new event definition.
            //
            def = evm_create_def(ctx, eid, formatter);
            if(def != NULL)
            {
                //
                // Use an atomic operation to add the defintion
                // to the head of the list.
                //
                ATOMIC_store(&ctx->defs, &def, &def->next);
                retval = true;
            }
        }
    }

    return retval;
}

evm_listener_t *evm_add_listener(evm_ctx_t *ctx, evm_eid eid, evm_event_cb cb, evm_listener_destroy_cb destroycb, void *user)
{
    //
    // Check if we are using the singleton.
    //
    if(ctx == NULL)
    {
        ctx = evm_singleton();
    }

    evm_listener_t *retval = NULL;
    if((ctx != NULL) && (cb != NULL))
    {
        evm_def_t *def = evm_find_def(ctx, eid);
        if(def != NULL)
        {
            retval = evm_create_listener(def, cb, destroycb, user);
            if(retval != NULL)
            {
                //
                // Read-lock while we modify the listener list. We can get
                // by with a read-lock since it will take a single
                // atomic operation to update the pointers.
                //
                pthread_rwlock_rdlock(&ctx->lock);

                //
                // Add the new listener to the head of the
                // listener list.
                //
                ATOMIC_store(&def->listeners, &retval, &retval->next);

                //
                // Release the read-lock.
                //
                pthread_rwlock_unlock(&ctx->lock);
            }
        }
    }

    return retval;
}

void evm_remove_listener(evm_ctx_t *ctx, evm_listener_t *listener)
{
    //
    // Check if we are using the singleton.
    //
    if(ctx == NULL)
    {
        ctx = evm_singleton();
    }

    if((ctx != NULL) && (listener != NULL))
    {
        //
        // Read-lock while we modify the listener. We can get
        // by with a read-lock since it will take a single
        // atomic operation to logically remove the listener.
        //
        pthread_rwlock_rdlock(&ctx->lock);

        //
        // Use an atomic operation to set the event callback
        // function to NULL. This will signify a logicially
        // removed listener.
        //
        evm_event_cb result;
        evm_event_cb val = NULL;
        ATOMIC_store(&listener->cb, &val, &result);

        //
        // Release the read-lock.
        //
        pthread_rwlock_unlock(&ctx->lock);
    }
}

evm_session_t *evm_start_session(evm_ctx_t *ctx,
                               evm_eid eid,
                               void *data,
                               evm_event_destroy_cb eventcb,
                               evm_session_cb cb,
                               void *user)
{
    evm_session_t *retval = NULL;

    //
    // Check if we are using the singleton.
    //
    if(ctx == NULL)
    {
        ctx = evm_singleton();
    }

    evm_def_t *def = evm_find_def(ctx, eid);
    if(def != NULL)
    {
        //
        // Add a session to the context.
        //
        retval = evm_create_session(ctx, cb, user);
        if(retval)
        {
            //
            // Push a new event group onto the session.
            //
            evm_event_grp_t *grp = evm_create_grp(retval, 0);
            if(evm_session_push_grp(retval, grp) != NULL)
            {
                //
                // Push an event onto the group.
                //
                evm_event_t *event = evm_create_event(def, grp, data, eventcb, user);
                if(evm_grp_push_event(grp, event))
                {
                    //
                    // Now that it is complete, push the session onto the queue.
                    // This will perform a short spin-lock.
                    //
                    if(evm_ctx_push_session(ctx, retval) != NULL)
                    {
                        //
                        // Signal any worker threads that a new session is available
                        // on the queue.
                        //
                        pthread_cond_signal(&ctx->condv);
                    }
                    else
                    {
                        FREE(event);
                        FREE(grp);
                        FREE(retval);
                        retval = NULL;
                    }
                }
                else
                {
                    FREE(grp);
                    FREE(retval);
                    retval = NULL;
                }
            }
            else
            {
                FREE(retval);
                retval = NULL;
            }
        }
    }

    return retval;
}

bool evm_cancel_session(evm_ctx_t *ctx, evm_session_t *session)
{
    bool retval = false;
    if(session != NULL)
    {
        //
        // Check if we are using the singleton.
        //
        if(ctx == NULL)
        {
            ctx = evm_singleton();
        }

        //
        // Short spin-lock while we search for the session
        // and potential remove it from the queue.
        //
        ATOMIC_spin(&ctx->qspin);

        //
        // Search for the session.
        //
        evm_session_t *curr = ctx->head;
        evm_session_t *prev = NULL;
        while(curr != NULL)
        {
            if(curr == session)
            {
                if(prev != NULL)
                {
                    prev->next = curr->next;
                }
                else
                {
                    ctx->head = curr->next;
                    if(ctx->head == NULL)
                    {
                        ctx->tail = NULL;
                    }
                }
                retval = true;

                break;
            }

            prev = curr;
            curr = curr->next;
        }

        //
        // Release the spin-lock.
        //
        ATOMIC_release(&ctx->qspin);

        //
        // If the session was found on the queue destroy
        // it.
        //
        if(retval)
        {
            //
            // Dispose of the session.
            //
            evm_destroy_session(session);
        }
    }

    return retval;
}

bool evm_session_append(evm_session_t *session, evm_eid eid, void *data, evm_event_destroy_cb destroycb, void *user)
{
    bool retval = false;
    if(session != NULL)
    {
        evm_def_t *def = evm_find_def(session->ctx, eid);
        if(def != NULL)
        {
            //
            // The event group at the back of the queue is the current
            // one for new session events.
            //
            evm_event_grp_t *grp = evm_session_back_grp(session);
            if(grp != NULL)
            {
                evm_event_t *event = evm_create_event(def, grp, data, destroycb, user);
                if(evm_grp_push_event(grp, event))
                {
                    retval = true;
                }
                else
                {
                    //
                    // Do not call evm_destroy_event as it will trigger callbacks.
                    //
                    FREE(event);
                }
            }
        }
    }

    return retval;
}

evm_ctx_t *evm_session_ctx(evm_session_t *session)
{
    evm_ctx_t *retval = NULL;
    if(session != NULL)
    {
        retval = session->ctx;
    }

    return retval;
}

void evm_session_process(evm_session_t *session)
{
    if(session != NULL)
    {
        //
        // Process the event groups for the session. An event group may
        // lead to the creation of additional event groups.
        //
        evm_event_grp_t *grp = NULL;
        bool run = true;
        while((run) && ((grp = evm_session_pop_grp(session)) != NULL))
        {
            //
            // Skip the group if it has no events.
            //
            if(grp->head != NULL)
            {
                //
                // Create a new event group to contain any events generated while
                // processing the current event group. This group will be one
                // level deeper than the current group.
                //
                evm_event_grp_t *ngrp = evm_create_grp(session, grp->depth+1);
                run = (evm_session_push_grp(session, ngrp) != NULL);

                //
                // Process all the events in the current event group.
                //
                evm_event_t *event = NULL;
                while((run) && ((event = evm_grp_pop_event(grp)) != NULL))
                {
                    //
                    // Dispatch the current event. Depending on the result
                    // of the event processing we either continue to process
                    // events or abort event processing.
                    //
                    run = evm_dispatch_event(event);

                    //
                    // Destroy the current event.
                    //
                    evm_destroy_event(event);
                }
            }

            //
            // Destroy the event group.
            //
            evm_destroy_grp(grp);
        }

        //
        // Release the session.
        //
        evm_destroy_session(session);
    }
}

bool evm_dispatch_event(evm_event_t *event)
{
    bool retval = true;

    if(event != NULL)
    {
        //
        // Mark the event as having been dispatched.
        //
        event->dispatched = true;

        //
        // Get the session from the event's group.
        //
        evm_session_t *session = event->grp->session;
        evm_ctx_t *ctx = session->ctx;

        //
        // Grab a read lock while we process the listeners for the current
        // event type.
        //
        pthread_rwlock_rdlock(&ctx->lock);

        //
        // Use an atomic operation to fetch the head of the listener
        // list.
        //
        evm_listener_t *listener;
        ATOMIC_fetch(&event->def->listeners, &listener);
        while(listener != NULL)
        {
            //
            // Get the listener's event callback funtion using an atomic operation. Listeners
            // that have been logically removed will have a NULL callback function. It is possible
            // for the callback to be set to NULL after this operation, but since it is a logical
            // removal, it won't matter.
            //
            evm_event_cb cb;
            ATOMIC_fetch(&listener->cb, &cb);
            if(cb != NULL)
            {
                //
                // Call the listener's callback function and report its result back to the
                // session.
                //
                bool result = cb(session, event, listener->user);
                if(session->cb != NULL)
                {
                    evm_session_callback_t cbdata;
                    cbdata.reason = eEvmListenerResult;
                    cbdata.result.event = event;
                    cbdata.result.val = result;
                    session->cb(session, &cbdata, session->user);
                }
            }

            listener = listener->next;
        }

        //
        // Release the lock.
        //
        pthread_rwlock_unlock(&ctx->lock);

        if(session->cb != NULL)
        {
            //
            // Notify that listener processing for the event has completed.
            //
            evm_session_callback_t cbdata;
            cbdata.reason = eEvmEventComplete;
            cbdata.event.event = event;
            cbdata.event.depth = event->grp->depth;
            cbdata.event.halt = false;
            session->cb(session, &cbdata, session->user);

            //
            // Determine if event processing should be
            // discontinued.
            //
            retval = !cbdata.event.halt;
        }
    }

    return retval;
}

evm_def_t *evm_find_def(evm_ctx_t *ctx, evm_eid eid)
{
    evm_def_t *retval = NULL;
    if(ctx != NULL)
    {
        //
        // Use an atomic operation to get the head of the definitions
        // list.
        ATOMIC_fetch(&ctx->defs, &retval);

        //
        // Walk the definitions looking for a matching
        // event type identifier.
        //
        while(retval != NULL)
        {
            if(retval->eid == eid)
            {
                break;
            }

            retval = retval->next;
        }
    }

    return retval;
}

evm_def_t *evm_create_def(evm_ctx_t *ctx, evm_eid eid, evm_event_formatter formatter)
{
    evm_def_t *retval = ALLOC(sizeof(*retval));
    if(retval != NULL)
    {
        memset(retval, 0, sizeof(*retval));
        retval->eid = eid;
        retval->ctx = ctx;
        retval->formatter = formatter;
    }

    return retval;
}

evm_listener_t *evm_create_listener(evm_def_t *def, evm_event_cb cb, evm_listener_destroy_cb destroycb, void *user)
{
    evm_listener_t *retval = ALLOC(sizeof(*retval));
    if(retval != NULL)
    {
        memset(retval, 0, sizeof(*retval));
        retval->def = def;
        retval->cb = cb;
        retval->destroycb = destroycb;
        retval->user = user;
    }

    return retval;
}

evm_session_t *evm_create_session(evm_ctx_t *ctx, evm_session_cb cb, void *user)
{
    evm_session_t *retval = ALLOC(sizeof(*retval));
    if(retval != NULL)
    {
        memset(retval, 0, sizeof(*retval));
        retval->ctx = ctx;
        retval->cb = cb;
        retval->user = user;
    }

    return retval;
}

evm_event_grp_t *evm_create_grp(evm_session_t *session, uint32_t depth)
{
    evm_event_grp_t *retval = ALLOC(sizeof(*retval));
    if(retval)
    {
        memset(retval, 0, sizeof(*retval));
        retval->session = session;
        retval->depth = depth;
    }

    return retval;
}

evm_event_t *evm_create_event(evm_def_t *def,
                              evm_event_grp_t *grp,
                              void *data,
                              evm_event_destroy_cb cb,
                              void *user)
{
    evm_event_t *retval = ALLOC(sizeof(*retval));
    if(retval != NULL)
    {
        memset(retval, 0, sizeof(*retval));
        retval->def = def;
        retval->grp = grp;
        retval->data = data;
        retval->destroycb = cb;
        retval->user = user;
    }

    return retval;
}


void evm_destroy_def(evm_def_t *def)
{
    if(def != NULL)
    {
        evm_listener_t *curr = def->listeners;
        while(curr != NULL)
        {
            evm_listener_t *tmp = curr->next;
            evm_destroy_listener(curr);
            curr = tmp;
        }
        FREE(def);
    }
}

void evm_destroy_listener(evm_listener_t *listener)
{
    if(listener != NULL)
    {
        //
        // Call the listener destroyed callback.
        //
        if((listener->destroycb != NULL) && (listener->def != NULL))
        {
            listener->destroycb(listener, listener->def->eid, listener->user);
        }

        FREE(listener);
    }
}

void evm_destroy_session(evm_session_t *session)
{
    if(session != NULL)
    {
        evm_event_grp_t *curr = session->head;
        while(curr != NULL)
        {
            evm_event_grp_t *tmp = curr->next;
            evm_destroy_grp(curr);
            curr = tmp;
        }

        if(session->cb != NULL)
        {
            evm_session_callback_t cbdata;
            cbdata.reason = eEvmSessionDestroy;
            session->cb(session, &cbdata, session->user);
        }

        FREE(session);
    }
}

void evm_destroy_grp(evm_event_grp_t *grp)
{
    if(grp != NULL)
    {
        evm_event_t *curr = grp->head;
        while(curr != NULL)
        {
            evm_event_t *tmp = curr->next;
            evm_destroy_event(curr);
            curr = tmp;
        }

        FREE(grp);
    }
}

void evm_destroy_event(evm_event_t *event)
{
    if(event != NULL)
    {
        if(event->destroycb != NULL)
        {
            event->destroycb(event, event->dispatched, event->user);
        }

        FREE(event->strep);
        FREE(event);
    }
}

evm_session_t *evm_ctx_push_session(evm_ctx_t *ctx, evm_session_t *session)
{
    if((ctx == NULL) || (session == NULL))
    {
        return NULL;
    }

    //
    // Add the session to the end of the queue.
    // We use a short spin-lock to ensure
    // synchronization.
    //
    ATOMIC_spin(&ctx->qspin);
    if(ctx->tail != NULL)
    {
        ctx->tail->next = session;
        ctx->tail = session;
    }
    else
    {
        ctx->head = session;
        ctx->tail = session;
    }

    //
    // Release the spin-lock.
    //
    ATOMIC_release(&ctx->qspin);

    return session;
}

evm_session_t *evm_ctx_pop_session(evm_ctx_t *ctx)
{
    evm_session_t *retval = NULL;
    if(ctx != NULL)
    {
        //
        // Pop the session at the head of the queue.
        // We use a short spin-lock to ensure
        // synchronization.
        //
        ATOMIC_spin(&ctx->qspin);
        retval = ctx->head;
        if(retval != NULL)
        {
            ctx->head = retval->next;
            if(ctx->head == NULL)
            {
                ctx->tail = NULL;
            }
        }

        //
        // Release the spin-lock.
        //
        ATOMIC_release(&ctx->qspin);

        //
        // Reset the next pointer.
        //
        if(retval != NULL)
        {
            retval->next = NULL;
        }
    }

    return retval;
}

evm_session_t *evm_ctx_front_session(evm_ctx_t *ctx)
{
    evm_session_t *retval = NULL;
    if(ctx != NULL)
    {
        retval = ctx->head;
    }

    return retval;
}

evm_session_t *evm_ctx_back_session(evm_ctx_t *ctx)
{
    evm_session_t *retval = NULL;
    if(ctx != NULL)
    {
        retval = ctx->tail;
    }

    return retval;
}

evm_event_grp_t *evm_session_push_grp(evm_session_t *session, evm_event_grp_t *grp)
{
    if((session == NULL) || (grp == NULL))
    {
        return NULL;
    }

    if(session->tail != NULL)
    {
        session->tail->next = grp;
        session->tail = grp;
    }
    else
    {
        session->head = grp;
        session->tail = grp;
    }

    return grp;
}

evm_event_grp_t *evm_session_front_grp(evm_session_t *session)
{
    evm_event_grp_t *retval = NULL;
    if(session != NULL)
    {
        retval = session->head;
    }

    return retval;
}

evm_event_grp_t *evm_session_back_grp(evm_session_t *session)
{
    evm_event_grp_t *retval = NULL;
    if(session != NULL)
    {
        retval = session->tail;
    }

    return retval;
}

evm_event_grp_t *evm_session_pop_grp(evm_session_t *session)
{
    evm_event_grp_t *retval = NULL;
    if(session != NULL)
    {
        retval = session->head;
        if(retval != NULL)
        {
            session->head = retval->next;
            retval->next = NULL;
            if(session->head == NULL)
            {
                session->tail = NULL;
            }
        }
    }

    return retval;
}

evm_event_t *evm_grp_push_event(evm_event_grp_t *grp, evm_event_t *event)
{
    if((grp == NULL) || (event == NULL))
    {
        return NULL;
    }

    if(grp->tail != NULL)
    {
        grp->tail->next = event;
        grp->tail = event;
    }
    else
    {
        grp->head = event;
        grp->tail = event;
    }

    return event;
}

evm_event_t *evm_grp_pop_event(evm_event_grp_t *grp)
{
    evm_event_t *retval = NULL;
    if(grp != NULL)
    {
        retval = grp->head;
        if(retval != NULL)
        {
            grp->head = retval->next;
            retval->next = NULL;
            if(grp->head == NULL)
            {
                grp->tail = NULL;
            }
        }
    }

    return retval;
}

evm_event_t *evm_grp_front_event(evm_event_grp_t *grp)
{
    evm_event_t *retval = NULL;
    if(grp != NULL)
    {
        retval = grp->head;
    }

    return retval;
}

evm_event_t *evm_grp_back_event(evm_event_grp_t *grp)
{
    evm_event_t *retval = NULL;
    if(grp != NULL)
    {
        retval = grp->tail;
    }

    return retval;
}

void evm_worker_cleanup(void *data)
{
    evm_ctx_t *ctx = (evm_ctx_t *)data;
    if(ctx != NULL)
    {
        pthread_mutex_unlock(&ctx->mutex);
    }
}

tp_code_t evm_worker(void *data)
{
    pthread_cleanup_push(evm_worker_cleanup, data);
    evm_ctx_t *ctx = (evm_ctx_t *)data;
    if(ctx != NULL)
    {
        evm_session_t *session = NULL;
        while(true)
        {
            //
            // Lock the mutex  while we check for events on the queue.
            //
            pthread_mutex_lock(&ctx->mutex);

            //
            // Get the next event session from the context block until one
            // becomes available. This will perform a short spin-lock.
            //
            while((session = evm_ctx_pop_session(ctx)) == NULL)
            {
                pthread_cond_wait(&ctx->condv, &ctx->mutex);
            }

            //
            // Unlock the mutex.
            //
            pthread_mutex_unlock(&ctx->mutex);

            //
            // Process the session.
            //
            evm_session_process(session);
        }
    }

    pthread_cleanup_pop(0);

    return TP_OK;
}

tp_code_t evm_maintenance(void *data)
{
    evm_ctx_t *ctx = (evm_ctx_t *)data;
    if(ctx != NULL)
    {
        while(true)
        {
            //
            // Sleep until the next maintenance cycle.
            //
            sleep(ctx->mfreq);

            //
            // Will contain the list of all dead listeners.
            //
            evm_listener_t *dead = NULL;

            //
            // Use an atomic operation to get the head of the event
            // type definitions list.
            //
            evm_def_t *def;
            ATOMIC_fetch(&ctx->defs, &def);

            //
            // Write-lock while we find and remove dead listeners.
            //
            pthread_rwlock_wrlock(&ctx->lock);

            //
            // Walk through the definitions.
            //
            while(def != NULL)
            {
                //
                // Walk through the list of listeners on the current
                // event type definition.
                //
                evm_listener_t *curr = def->listeners;
                evm_listener_t *prev = NULL;
                while(curr != NULL)
                {
                    evm_listener_t *next = curr->next;

                    //
                    // A logically deleted listener will have a NULL callback.
                    // Use an atomic operation here to retrieve the callback
                    // function
                    //
                    if(curr->cb == NULL)
                    {
                        //
                        // Use an atomic operation to remove the listener from
                        // the event typed listener list.
                        //
                        if(prev != NULL)
                        {
                            prev->next = curr->next;
                        }
                        else
                        {
                            def->listeners = curr->next;
                        }

                        //
                        // Add the removed listener to the dead list.
                        //
                        curr->next = dead;
                        dead = curr;
                    }
                    else
                    {
                        prev = curr;
                    }

                    //
                    // Move to the next listener.
                    //
                    curr = next;
                }

                //
                // Move to the next definition.
                //
                def = def->next;
            }

            //
            // Release the write-lock.
            //
            pthread_rwlock_unlock(&ctx->lock);

            //
            // Deallocate the dead listeners.
            //
            while(dead != NULL)
            {
                evm_listener_t *tmp = dead->next;
                evm_destroy_listener(dead);
                dead = tmp;
            }
        }
    }

    return TP_OK;
}
