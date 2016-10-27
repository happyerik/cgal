include = function (filename) {
    var basename = __FILE__.replace(RegExp('/[^/]*$'), "")
    main_window.loadScript(basename + "/" + filename)
}

print_backtrace = function(bt, prefix) {
    var p = typeof prefix !== 'undefined' ?  prefix : "  ";
    for(var i = 0; i < bt.length; ++i) {
        print(p + bt[i])
    }
}

print_exception_and_bt = function(e) {
    print("Caught exception in " + __FILE__ + ": " + e)
    print("Backtrace:")
    print_backtrace(e.backtrace)
}

quit = main_window.quit

noop = function () {}

noop()
