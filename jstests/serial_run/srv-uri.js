(function() {
    "use strict";
    const md = MongoRunner.runMongod({port: "27017", dbpath: MongoRunner.dataPath});
    assert.neq(null, md, "unable to start mongerd");
    const targetURI = 'mongerdb+srv://test1.test.build.10gen.cc./?ssl=false';
    const exitCode = runMongoProgram('monger', targetURI, '--eval', ';');
    assert.eq(exitCode, 0, "Failed to connect with a `mongerdb+srv://` style URI.");
    MongoRunner.stopMongod(md);
})();
