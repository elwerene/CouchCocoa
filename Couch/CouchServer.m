//
//  CouchServer.m
//  CouchCocoa
//
//  Created by Jens Alfke on 5/26/11.
//  Copyright 2011 Couchbase, Inc. All rights reserved.
//

#import "CouchServer.h"

#import "CouchInternal.h"
#import "RESTCache.h"


static NSString* const kLocalServerURL = @"http://127.0.0.1:5984/";


@implementation CouchServer


- (id) initWithURL: (NSURL*)url {
    return [super initWithURL: url];
}


/** Without URL, connects to localhost on default port */
- (id) init {
    return [self initWithURL:[NSURL URLWithString: kLocalServerURL]];
}


- (void)dealloc {
    [_dbCache release];
    [super dealloc];
}


- (RESTResource*) childWithPath: (NSString*)name {
    return [[[CouchResource alloc] initWithParent: self relativePath: name] autorelease];
}


- (NSString*) getVersion: (NSError**)outError {
    RESTOperation* op = [self GET];
    [op wait];
    if (outError)
        *outError = op.error;
    return [[op.responseBody.fromJSON objectForKey: @"version"] description]; // Blocks!
}


- (NSArray*) generateUUIDs: (NSUInteger)count {
    NSDictionary* params = [NSDictionary dictionaryWithObject:
                                    [NSNumber numberWithUnsignedLong: count]
                                                       forKey: @"?count"];
    RESTOperation* op = [[self childWithPath: @"_uuids"] sendHTTP: @"GET" parameters: params];
    return [op.responseBody.fromJSON objectForKey: @"uuids"];
}


- (NSArray*) getDatabases {
    RESTOperation* op = [[self childWithPath: @"_all_dbs"] GET];
    NSArray* names = $castIf(NSArray, op.responseBody.fromJSON); // Blocks!
    return [names rest_map: ^(id name) {
        return [name isKindOfClass:[NSString class]] ? [self databaseNamed: name] : nil;
    }];
}


- (CouchDatabase*) databaseNamed: (NSString*)name {
    CouchDatabase* db = (CouchDatabase*) [_dbCache resourceWithRelativePath: name];
    if (!db) {
        db = [[CouchDatabase alloc] initWithParent: self relativePath: name];
        if (!db)
            return nil;
        if (!_dbCache)
            _dbCache = [[RESTCache alloc] initWithRetainLimit: 0];
        [_dbCache addResource: db];
        [db release];
    }
    return db;
}


@end
