//
//  CouchDatabase.h
//  CouchCocoa
//
//  Created by Jens Alfke on 5/26/11.
//  Copyright 2011 Couchbase, Inc. All rights reserved.
//

#import "CouchResource.h"
@class RESTCache, CouchChangeTracker, CouchDocument, CouchDesignDocument, CouchQuery, CouchServer;
struct CouchViewDefinition;


/** Type of block that's called when the database changes. */
typedef void (^OnDatabaseChangeBlock)(CouchDocument*);


/** Option flags for replication (push/pull). */
enum {
    kCouchReplicationCreateTarget = 1,  /**< Create the destination database if it doesn't exist */
    kCouchReplicationContinuous   = 2,  /**< Continuous mode; remains active till canceled */
    kCouchReplicationCancel       = 4   /**< Cancel a current replication in progress */
};
typedef NSUInteger CouchReplicationOptions;


/** A CouchDB database; contains CouchDocuments.
    The CouchServer is the factory object for CouchDatabases. */
@interface CouchDatabase : CouchResource
{
    @private
    RESTCache* _docCache;
    NSCountedSet* _busyDocuments;
    CouchChangeTracker* _tracker;
    NSUInteger _lastSequenceNumber;
    BOOL _lastSequenceNumberKnown;
    OnDatabaseChangeBlock _onChange;
    NSMutableArray* _deferredChanges;
}

/** A convenience to instantiate a CouchDatabase directly from a URL, without having to first instantiate a CouchServer.
    Unlike CouchServer's -databaseNamed: method, if you call this twice with the same URL you _will_ get two distinct CouchDatabase objects (with two distinct CouchServers as parents.) */
+ (CouchDatabase*) databaseWithURL: (NSURL*)databaseURL;

@property (readonly) CouchServer* server;

/** Creates the database on the server.
    Fails with an HTTP status 412 (Conflict) if a database with this name already exists. */
- (RESTOperation*) create;

/** Gets the current total number of documents. (Synchronous) */
- (NSInteger) getDocumentCount;

/** Instantiates a CouchDocument object with the given ID.
    Makes no server calls; a document with that ID doesn't even need to exist yet.
    CouchDocuments are cached, so there will never be more than one instance (in this database)
    at a time with the same documentID. */
- (CouchDocument*) documentWithID: (NSString*)docID;

/** Creates a CouchDocument object with no current ID.
    The first time you PUT to that document, it will be created on the server (via a POST). */
- (CouchDocument*) untitledDocument;

/** Returns a query that will fetch all documents in the database. */
- (CouchQuery*) getAllDocuments;

/** Returns a query that will fetch the documents with the given IDs. */
- (CouchQuery*) getDocumentsWithIDs: (NSArray*)docIDs;

/** Bulk-writes multiple documents in one HTTP call.
    Documents that don't exist on the server yet will be created. */
- (RESTOperation*) putChanges: (NSArray*)properties toRevisions: (NSArray*)revisions;

/** Bulk-writes multiple documents in one HTTP call.
    Each property dictionary with an "_id" key will update the existing document with that ID, or create a new document with that ID. A dictionary without an "_id" key will always create a new document with a server-assigned unique ID.
    If a dictionary updates an existing document, it must also have an "_rev" key that contains the document's current revision ID.
    The write is asynchronous, but after the returned operation finishes, its -resultObject will be an NSArray of CouchDocuments.
    @param properties  Array of NSDictionaries, each one the properties of a document. */
- (RESTOperation*) putChanges: (NSArray*)properties;

/** Empties the cache of recently used CouchDocument objects.
    API calls will now instantiate and return new instances. */
- (void) clearDocumentCache;

#pragma mark QUERIES & DESIGN DOCUMENTS:

/** Returns a query that runs custom map/reduce functions.
    This is very slow compared to a precompiled view and should only be used for testing. */
- (CouchQuery*) slowQueryWithViewDefinition:(struct CouchViewDefinition)definition;

/** Convenience method that creates a custom query from a JavaScript map function. */
- (CouchQuery*) slowQueryWithMapFunction: (NSString*)mapFunctionSource;

/** Instantiates a CouchDesignDocument object with the given ID.
    Makes no server calls; a design document with that ID doesn't even need to exist yet.
    CouchDesignDocuments are cached, so there will never be more than one instance (in this database) at a time with the same name. */
- (CouchDesignDocument*) designDocumentWithName: (NSString*)name;

#pragma mark CHANGE TRACKING:

/** Controls whether document change-tracking is enabled.
    It's off by default.
    Only external changes are tracked, not ones made through this database object and its children. This is useful in handling synchronization, or multi-client access to the same database, or on application relaunch to detect changes made after it last quit.
    Turning tracking on creates a persistent socket connection to the database, and will post potentially a lot of notifications, so don't turn it on unless you're actually going to use the notifications. */
@property BOOL tracksChanges;

/** The last change sequence number received from the database.
    If this is not known yet, the current value will be fetched via a synchronous query.
    You can save the current value on quit, and restore it on relaunch before enabling change tracking, to get notifications of all changes that have occurred in the meantime. */
@property NSUInteger lastSequenceNumber;

#pragma mark REPLICATION & SYNCHRONIZATION:

/** Triggers replication from a source database, to this database.
    @param sourceURL  The URL of the database to replicate from.
    @param options  Zero or more option flags affecting the replication.
    @return  A RESTOperation that will complete when the replication finishes. The response body will be a JSON object describing what occurred. */
- (RESTOperation*) pullFromDatabaseAtURL: (NSURL*)sourceURL 
                                 options: (CouchReplicationOptions)options;

/** Triggers replication from this database to a target database.
    @param targetURL  The URL of the database to replicate to.
    @param options  Zero or more option flags affecting the replication.
    @return  A RESTOperation that will complete when the replication finishes. The response body will be a JSON object describing what occurred. */
- (RESTOperation*) pushToDatabaseAtURL: (NSURL*)targetURL
                               options: (CouchReplicationOptions)options;


@end


/** This notification is posted by a CouchDatabase in response to an external change (as reported by the _changes feed.)
    It is not sent in response to 'local' changes made by its child objects.
    It will not be sent unless tracksChanges is enabled. */
extern NSString* const kCouchDatabaseChangeNotification;
