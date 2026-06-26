# Perl Binding for libhttp2 (AnyEvent)

This document describes the Perl binding for libhttp2, designed to integrate cleanly with [AnyEvent](https://metacpan.org/pod/AnyEvent).

## Installation

```bash
cpanm AnyEvent::HTTP2
# or
perl Makefile.PL && make && make install
```

## Basic Usage

```perl
use AnyEvent;
use AnyEvent::HTTP2;

my $cv = AE::cv;

my $h2 = AnyEvent::HTTP2->new(
    host => 'example.com',
    port => 443,
    tls  => 1,
    on_event => sub {
        my ($self, $event) = @_;
        if ($event->{type} eq 'headers') {
            print "Got headers on stream $event->{stream_id}\n";
        }
        elsif ($event->{type} eq 'data') {
            print "Data: ", $event->{data}, "\n";
        }
    },
);

$h2->connect->then(sub {
    $h2->request(
        method => 'GET',
        path   => '/',
        headers => [ ':authority' => 'example.com' ],
    );
})->then(sub {
    $cv->send;
});

$cv->recv;
```

## Architecture

The Perl binding uses **FFI::Platypus** to call the C library. This approach:

- Avoids fragile XS code
- Is easier to maintain
- Aligns with ADR 010's emphasis on FFI-friendly interfaces

The binding provides two layers:

1. **Low-level** (`AnyEvent::HTTP2::Raw`) — direct mapping to C functions.
2. **High-level** (`AnyEvent::HTTP2`) — AnyEvent-friendly object with callbacks and promises.

## AnyEvent Integration

The binding registers file descriptors with AnyEvent's I/O watchers. When the C library produces output via `http2_get_output`, the Perl side writes it using an `AnyEvent::Handle`.

Incoming data is fed via `http2_feed_input` when AnyEvent reports readability.

## Event Mapping

`protocol_event_t` is mapped to Perl hashrefs:

```perl
{
    protocol => 'http2',
    type     => 'data',           # or 'headers', 'error', etc.
    stream_id => 1,
    data      => '...',
    end_stream => 0,
}
```

## Configuration

```perl
my $h2 = AnyEvent::HTTP2->new(
    event_queue_size => 32,
    # ... other options
);
```

This maps to `http2_config_t`.

## Future Language Bindings

The same C API patterns used here (opaque handles, `next_event`, config structs) will be reused for Rust, Python, Go, and Zig bindings. See `docs/language-bindings.md` for general guidance.