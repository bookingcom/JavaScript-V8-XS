use strict;
use warnings;

use Test::More;

my $CLASS = 'JavaScript::V8::XS';

# A Perl exception raised inside a function that JS calls back into must never
# croak() through V8's C++ scopes.  Before the fix that left the isolate
# "entered": the next VM destruction aborted the whole process inside
# v8::Isolate::Dispose(), and using the VM again first crashed in the next GC.
#
# The contract for the Perl caller is unchanged: $vm->eval() still dies with
# "Perl sub died with error: ...", but only after V8 has unwound.  Inside JS
# the Perl error is a regular exception.

sub test_perl_die_still_reaches_the_perl_caller {
    my $vm = $CLASS->new({ save_messages => 1 });
    $vm->set('boom', sub { die "kaboom\n" });
    $vm->set('fine', sub { return 'ok' });

    my $ok = eval { $vm->eval('var caught; try { boom() } catch (e) { caught = e.message }; "done"', 'die1.js'); 1 };
    ok(!$ok, 'eval() dies when a callback dies');
    like($@, qr/^Perl sub died with error: kaboom/, 'with the same message as before');
    like($vm->get('caught'), qr/^Perl sub died with error: kaboom/, 'and inside JS the Perl error was a catchable exception');
    is($vm->eval('fine()', 'die1b.js'), 'ok', 'VM still usable afterwards');

    $ok = eval { $vm->eval('boom(); "not reached"', 'die2.js'); 1 };
    ok(!$ok, 'an uncaught Perl exception also dies in eval()');
    like($@, qr/^Perl sub died with error: kaboom/, 'with the Perl error');
    my $msgs = $vm->get_msgs();
    like(join('', @{ $msgs->{stderr} // [] }), qr/kaboom/, 'and the uncaught JS exception was reported like any other');
    $vm->reset_msgs();
    undef $vm;
}

sub test_vm_destroy_after_dying_callback {
    my $vm;
    foreach my $round (1..3) {
        undef $vm;
        $vm = $CLASS->new({ save_messages => 1 });
        $vm->set('boom', sub { die "again\n" });
        eval { $vm->eval('boom()', 'die3.js') };
        is($vm->eval('1+1', 'die3b.js'), 2, "round $round: VM works after a dying callback and a VM destruction");
    }
    undef $vm;
    pass('destroying a VM after a dying callback does not abort');
}

sub test_vm_reuse_after_dying_callback {
    my $vm = $CLASS->new({ save_messages => 1 });
    $vm->set('boom', sub { die "yet again\n" });
    $vm->set('big', sub { return { map { ($_ => [1..100]) } 1..200 } });
    eval { $vm->eval('boom()', 'die4.js') };
    foreach my $round (1..30) {
        $vm->eval('big(); for (var k = 0; k < 2000; k++) { new Array(1000).fill(k) }', 'die4b.js');
    }
    $vm->run_gc();
    pass('heavy use and GC after a dying callback do not crash');
    undef $vm;
}

sub test_die_inside_event_loop_callback {
    my $vm = $CLASS->new({ save_messages => 1 });
    $vm->set('boom', sub { die "in the loop\n" });
    $vm->eval('function render() { setTimeout(function () { boom() }, 1) }', 'loop.js');
    my $ok = eval { $vm->dispatch_function_in_event_loop('render'); 1 };
    ok(!$ok, 'a callback dying inside the event loop dies in dispatch_function_in_event_loop()');
    like($@, qr/^Perl sub died with error: in the loop/, 'with the Perl error');
    is($vm->eval('2+2', 'loop2.js'), 4, 'VM still usable afterwards');
    undef $vm;
}

sub test_argument_errors {
    my $vm = $CLASS->new({ save_messages => 1 });
    $vm->set('cb', sub { return defined $_[0] ? 'defined' : 'undef' });
    is($vm->eval('cb(function(){})', 'arg1.js'), 'undef', 'plain JS function argument converts to undef, not garbage');
    is($vm->eval('cb(cb)', 'arg2.js'), 'defined', 'Perl-backed JS function argument converts to the Perl sub');
    my $ok = eval { $vm->eval('cb(Symbol("x"))', 'arg3.js'); 1 };
    ok(!$ok, 'an unconvertible argument is an error');
    like($@, qr/Don't know how to deal with this thing/, 'reported to the Perl caller without a croak through V8');
    is($vm->eval('cb(1)', 'arg4.js'), 'defined', 'VM still usable afterwards');
    undef $vm;
}

sub main {
    use_ok($CLASS);
    test_perl_die_still_reaches_the_perl_caller();
    test_vm_destroy_after_dying_callback();
    test_vm_reuse_after_dying_callback();
    test_die_inside_event_loop_callback();
    test_argument_errors();
    done_testing;
    return 0;
}

exit main();
