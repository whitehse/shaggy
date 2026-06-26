package AnyEvent::HTTP2;

use strict;
use warnings;
use FFI::Platypus 2.0;
use FFI::Platypus::Memory qw(malloc free);
use AnyEvent;
use AnyEvent::Handle;
use Carp qw(croak);

our $VERSION = '0.01';

# Load the C library
my $ffi = FFI::Platypus->new(api => 2);
$ffi->lib('libhttp2.so');   # Adjust path as needed

# Opaque types
$ffi->type('opaque' => 'http2_ctx_t');
$ffi->type('opaque' => 'http2_config_t');

# Enums
$ffi->type('int' => 'http2_role_t');

# protocol_event_t (simplified mapping)
$ffi->type('record(8)' => 'protocol_event_t');  # placeholder size

# Function declarations (matching ADR 010 style)
$ffi->attach( http2_create_with_config => ['http2_role_t', 'opaque'] => 'http2_ctx_t' );
$ffi->attach( http2_destroy            => ['http2_ctx_t'] => 'void' );
$ffi->attach( http2_feed_input         => ['http2_ctx_t', 'opaque', 'size_t'] => 'size_t' );
$ffi->attach( http2_next_event         => ['http2_ctx_t', 'opaque'] => 'int' );
$ffi->attach( http2_get_output         => ['http2_ctx_t', 'opaque', 'size_t'] => 'size_t' );

sub new {
    my ($class, %args) = @_;

    my $self = bless {
        ctx       => undef,
        on_event  => $args{on_event} // sub {},
        handle    => undef,
        host      => $args{host} // 'localhost',
        port      => $args{port} // 443,
        tls       => $args{tls} // 1,
    }, $class;

    # Create config (simplified)
    my $cfg = undef;  # In real code, allocate http2_config_t

    $self->{ctx} = http2_create_with_config(1, $cfg);  # 1 = SERVER role
    croak("Failed to create http2 context") unless $self->{ctx};

    return $self;
}

sub connect {
    my ($self) = @_;

    my $cv = AE::cv;

    $self->{handle} = AnyEvent::Handle->new(
        connect  => [ $self->{host}, $self->{port} ],
        on_error => sub { croak $_[2] },
        on_connect => sub {
            $cv->send($self);
        },
        on_read => sub {
            my ($h, $bufref) = @_;
            my $len = length $$bufref;
            my $ptr = $ffi->cast('string' => 'opaque', $$bufref);
            http2_feed_input($self->{ctx}, $ptr, $len);
            $$bufref = '';

            # Pump events
            $self->_pump_events();
        },
    );

    return $cv;
}

sub _pump_events {
    my ($self) = @_;
    my $event = {};  # In real code, map protocol_event_t

    while (http2_next_event($self->{ctx}, $event) == 0) {
        $self->{on_event}->($self, $event);
    }
}

sub request {
    my ($self, %args) = @_;
    # Placeholder for sending HEADERS + DATA frames
    # Real implementation would build HTTP/2 frames and call get_output
    warn "request() not fully implemented in skeleton";
}

sub DESTROY {
    my ($self) = @_;
    http2_destroy($self->{ctx}) if $self->{ctx};
}

1;

__END__

=head1 NAME

AnyEvent::HTTP2 - HTTP/2 client/server for AnyEvent

=head1 SYNOPSIS

    use AnyEvent::HTTP2;

    my $h2 = AnyEvent::HTTP2->new(
        host => 'example.com',
        on_event => sub { ... },
    );

    $h2->connect->then(sub { ... });

=head1 DESCRIPTION

Perl binding for libhttp2 using FFI, designed for seamless integration with AnyEvent.

See C<docs/perl-binding.md> for full documentation.

=cut
