#! /usr/bin/env ruby
#library for displaying data
require 'vizkit'
require 'readline'
require 'eigen'
require 'rock/bundle'

if !ARGV[0]  then 
    puts "usage: replay.rb log_dir"
    exit
end

logdir = ARGV[0]

#load log file 
log = Orocos::Log::Replay.open(File.join( logdir, "camera.0.log" ))

Orocos::CORBA::max_message_size = 100000000
Bundles.initialize
Nameservice::enable(:Local)
#Vizkit::ReaderWriterProxy.default_policy= Hash.new

Bundles.run "streamerTest", :valgrind => false, :output => nil do |p|
    encoder = Bundles.get('encoder')
    decoder = Bundles.get('decoder')
    log.camera_left.frame.connect_to( encoder.raw_pictures, :type => :buffer, :size => 3 );
    
    encoder.configure()
    encoder.start()
    encoder.mpeg_stream.connect_to(decoder.mpeg_stream, :type => :buffer, :size => 1000 )
    
    decoder.configure()
    decoder.start()
    
    Orocos.log_all_ports()
    
    
    Vizkit.control log
    Vizkit.exec()


end