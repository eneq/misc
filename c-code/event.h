#ifndef __EVENT_H__
#define __EVENT_H__

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>


/**
 * @file
 *
 * Event Manager.
 *
 * The event manager provides a mechanism for dispatching events to interested
 * listeners. Dispatching of events is done via a multi-threaded, worker
 * queue to keep the load on the event initiators as light as possible. The
 * number of event dispatching threads is configurable.
 *
 * Event types defined by and registered with a unique identifier. Listeners
 * can be added to the defined events. Event dispatching occurs within an event
 * session. Each triggered event has its own session. Within a session,
 * additional events may be triggered. Such events are still owned by the
 * session. For an event, each listener's event handler will be called.
 * The result of each handler is provided back to the session. After
 * all listeners have executed, the session can determine, via callback,
 * whether or not to continue to dispatch any of the new events generated
 * by the listeners.
 *
 *    |-----------------------------------------------------------
 *    | Session: S0                                              |
 *    |                                                          |
 *    |                       Event Group 1        Event Group 2 |
 *    |                                                          |
 *    |  Level 0                 Level 1             Level 2     |
 *    |                           *****               *****      |
 *    |                       |---*E01*------ L8 -----*E20*      |
 *    |                       |   *   *               *   *      |
 *    |                       |   *   *               *   *      |
 *    |                       |   *   *               *   *      |
 *    |            -- L0 ---------*E02*               *   *      |
 *    |            |              *   *               *   *      |
 *    |            |              *   *               *   *      |
 *    |   ****     |              *   *               *   *      |
 *    |   *E0*------- L1 ---------*E03*------ L4 -----*E21*      |
 *    |   ****     |              *   *               *****      |
 *    |            |              *   *                          |
 *    |            |              *   *                          |
 *    |            -- L2 ---------*E04*                          |
 *    |                           *****                          |
 *    |                                                          |
 *    |-----------------------------------------------------------
 *
 * In the example above, the session processing begins with the
 * dispatching of event E0 to its registered listeners [L0, L1, L2].
 * During listener processing, a new group of events are generated
 * [E01, E02, E03, E04]. Once all the listener event handlers for
 * event E0 are complete the session determines whether or not
 * to continue processing the next event group [E01, E02, E03, E04].
 *
 * If processing continues, each event in event group 1
 * are dispatched to their listeners. In the example, two
 * new events are generated in a new event group. Once all
 * event/listener processing for event group 1 are complete,
 * the session again determines whether or not to continue
 * processing with event group 2.
 *
 * Note this process, in theory, could proceed indefinitely unless
 * no new events are generated or the session explicitly halts event
 * dispatching.
 *
 * Event listeners can be added or removed at any point in time with
 * little impact on execution. In the case of removing an event listener,
 * once removed the listener's callback function will no longer be called
 * in response to subscribed events, but the actual removal of the listener
 * is deferred. A maintenance thread runs periodically to remove any
 * logically removed listeners. It is at this point, if provided, a listener
 * destroyed callback will be invoked.
 *
 */

typedef uint32_t evm_eid; /*!< Event type identifier. */
typedef struct evm_ctx_s evm_ctx_t; /*!< Event context type. */
typedef struct evm_session_s evm_session_t; /*!< Event session type. */
typedef struct evm_listener_s evm_listener_t; /*!< Event listener type. */
typedef struct evm_event_s evm_event_t; /*!< Event type. */

/**
 * @brief Event listener callback.
 *
 * Parameters are:
 * - The session to which the event belongs.
 * - The event.
 * - User-defined data.
 *
 * The callback should return true if the event was successfully handled.
 *
 */
typedef bool (*evm_event_cb)(evm_session_t *, evm_event_t *event, void* /*user data*/);

/**
 * @brief Event data to string formatter function type.
 *
 * Generates the string representation of the data for an event. The format of
 * the data is determined by the registrar.
 *
 * Parameters are:
 * - The event.
 * - The event data.
 * - The output character buffer.
 * - The size of of the output buffer.
 */
typedef void (*evm_event_formatter)(evm_event_t *,  char * /* buffer */, size_t /*buffer size */);

/**
 * @brief Event listener destroy callback.
 *
 * Calling evm_remove_listener() simply marks the listener for removal, but actual
 * destruction of the listener will not occur until the maintenance thread executes.
 *
 * The destroy callback will also be called when the evm_destroy() is called.
 *
 * Parameters are:
 * - The session to which the listener was registered.
 * - The listener being removed.
 * - the event id the listener was registered to.
 * - User-defined data.
 */
typedef void(*evm_listener_destroy_cb)(evm_listener_t *, evm_eid, void*);

/**
 * @brief Event destroy callback.
 *
 * Called when an event is about to be destroyed.
 *
 * Parameters are:
 * - The event.
 * - Whether or not it was dispatched.
 * - User-defined data.
 */
typedef void (*evm_event_destroy_cb)(evm_event_t *, bool /*dispatched*/, void* /*user data*/);

/**
 * @brief Session callback reasons.
 */
typedef enum evm_session_reason
{
    eEvmListenerResult, /*!< A listener has completed with an event result. */
    eEvmEventComplete, /*!< All listeners have completed their event processing. */
    eEvmSessionDestroy, /*!< The session is about to be destroyed. */
} evm_session_reason_t;

/**
 * @brief A listener event callback result.
 */
typedef struct evm_session_result_s
{
    evm_event_t *event; /*!< The event. */
    bool val; /*!< The listener event processing result. */
} evm_session_result_t;

/**
 * @brief Event processing complete.
 *
 * The session callback should set the "halt" member
 * to true if event processing should be aborted.
 *
 */
typedef struct evm_session_event_s
{
    evm_event_t *event; /*!< The event. */
    uint32_t depth; /*!< The event depth. */
    bool halt; /*!< Should be set by the callback to halt event processing. */
} evm_session_event_t;

/**
 * @brief Session callback data.
 *
 * The reason value can be used to determine which structure
 * in the union applies to the current callback.
 */
typedef struct evm_session_callback_s
{
    evm_session_reason_t reason; /*!< The reason for the callback. */
    union
    {
        evm_session_result_t result; /*!< reason == kListenerResult. */
        evm_session_event_t event; /*!< reason == kListenerEventComplete. */
    };
} evm_session_callback_t;

/**
 * @brief Session callback function type.
 *
 * Parameters are:
 * - The session.
 * - The session callback data.
 * - User-defined data.
 */
typedef void (*evm_session_cb)(evm_session_t *, evm_session_callback_t *, void* /* user data */);

/**
 * @brief Get the event manager singleton context.
 *
 * Passing NULL to any of the event manager functions will automatically
 * use the singleton. The singleton is created if it doesn't already
 * exist.
 *
 * @return - The singleton context.
 */
evm_ctx_t *evm_singleton();

/**
 * @brief Create an new event context.
 *
 * The number of worker threads determines how many event sessions
 * can be processed simultaneously. This value must be greater than
 * zero.
 *
 * @param nworkers - Number of session worker threads.
 * @param mfreq - How often the maintenance thread should execute (in seconds).
 *
 * @return - The event contex or NULL if an error occurred.
 */
evm_ctx_t *evm_initialize(size_t nworkers, uint32_t mfreq);

/**
 * @brief Destroy an event context.
 *
 * All sessions will be destroyed. Session and event destroyed callbacks
 * will be invoked.
 *
 * @param ctx - Context to be destroyed. NULL will destory the singleton.
 */
void evm_destroy(evm_ctx_t *ctx);

/**
 * @brief Register a new event type.
 *
 * @param ctx - Event context. A value of NULL will use the singleton.
 * @param eid - Event type identifier.
 * @param formatter - The event data to string formatter function..
 *
 * @return - True if the event type was successfully registered.
 */
bool evm_register_type(evm_ctx_t *ctx, evm_eid eid, evm_event_formatter formatter);

/**
 * @brief Add an listener to an event type.
 *
 * The listener will be called whenever an event of the
 * specified type occurs.
 *
 * @param ctx - Event context. A value of NULL will use the singleton.
 * @param eid - Event type identifier.
 * @param cb - Listener event callback.
 * @param destroycb - Listener destroy callback.
 * @param user - User-defined data.
 *
 * @return - True if the listener was successfully added.
 */
evm_listener_t *evm_add_listener(evm_ctx_t *ctx, evm_eid eid, evm_event_cb cb, evm_listener_destroy_cb destroycb, void *user);

/**
 * @brief Remove an event listener.
 *
 * Removing the listener will ensure it will not receive future events, but the listener will
 * exist until the maintenance thread executes. The maintenance thread will call the registered
 * destroy callback for the listener prior to actual deallocation.
 *
 * The destroy callback will also be called for all registered listeners when evm_destroy()
 * is called.
 *
 * @param ctx - Event context. A value of NULL will use the singleton.
 * @param listener - Listener to be removed.
 *
 * @return - True if the listener was successfully removed.
 */
void evm_remove_listener(evm_ctx_t *ctx, evm_listener_t *listener);

/**
 * @brief Start a new event session.
 *
 * @param ctx - Event context. A value of NULL will use the singleton.
 * @param eid - The event type identifier.
 * @param data - The event data.
 * @param eventcb - The event destroyed callback function.
 * @param cb - The session callback function.
 * @param user - User-defined callback data.
 *
 * @return - The new event session or NULL if it could not be started.
 */
evm_session_t *evm_start_session(evm_ctx_t *ctx,
                                 evm_eid eid,
                                 void *data,
                                 evm_event_destroy_cb eventcb,
                                 evm_session_cb cb,
                                 void *user);

/**
 * @brief Cancel a queued event session.
 *
 * This method will fail if the session is currently being processed or does
 * not exist in the context. If the session is successfully cancelled, its
 * destroyed callback will be called along with the destroy callbacks for
 * queued up events associated with the session.
 *
 * @param ctx - Event context. A value of NULL will use the singleton.
 * @param session - The session to be cancelled.
 *
 * @return - True if the session was removed.
 */
bool evm_cancel_session(evm_ctx_t *ctx, evm_session_t *session);

/**
 * @brief Append an event to an existing session.
 *
 * The added event will be added as a descendent of the current session
 * event.
 *
 * @param session - The event session.
 * @param eid - The current event type identifier.
 * @param data - The current event data.
 * @param destroycb - The event destroyed callback.
 * @param user - User-defined data.
 *
 * @return - True if the descendent event was successfully added.
 */
bool evm_session_append(evm_session_t *session, evm_eid eid, void *data, evm_event_destroy_cb destroycb, void *user);

/**
 * @brief Get the context associated with a session.
 *
 * @param session - The session to be queried.
 *
 * @return - The session context or NULL if an error occurred.
 */
evm_ctx_t *evm_session_ctx(evm_session_t *session);

/**
 * @brief Get the event type identifier.
 *
 * @param event - The event.
 *
 * @return - The type identifier.
 */
evm_eid evm_event_id(evm_event_t *event);

/**
 * @brief Get the event data.
 *
 * @param event - The event.
 *
 * @return The data for the event.
 */
void *evm_event_data(evm_event_t *event);

/**
 * @brief Get the event data string representation.
 *
 * If the event data does not have a string representation
 * defined, the empty string will be returned.
 *
 * @param event - The event.
 *
 * @return The strip representation or NULL if an error occurred.
 */
const char *evm_event_strep(evm_event_t *event);


/**
 * @brief Get the numerical representation of the given event name
 *
 * @param event - The even
 *
 * @return The strip representation or NULL if an error occurred.
 */
evm_eid evm_generate_eid(char* event);

#endif
