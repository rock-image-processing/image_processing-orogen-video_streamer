#! /usr/bin/env ruby
# -*- coding: utf-8 -*-

require 'orocos'
include Orocos
require 'readline'
require 'vizkit'


if !ARGV[0]  then 
    puts "usage: hostname/ip"
    exit
end
Orocos::CORBA.name_service = ARGV[0]
Orocos::CORBA::max_message_size = 100000000
Orocos.initialize

# setup the environment so that ruby can find the test deployment
ENV['PKG_CONFIG_PATH'] = "#{File.expand_path("..", File.dirname(__FILE__))}/build:#{ENV['PKG_CONFIG_PATH']}"

Orocos.run('video_streamer::VideoDecoder' => 'decoder', :valgrind => false) do
    encoder = TaskContext.get('encoder')
    decoder = TaskContext.get('decoder')

    encoder.mpeg_stream.connect_to(decoder.mpeg_stream, :type => :data )

    Vizkit.display(decoder.raw_pictures)

    decoder.configure()
    decoder.start()
    
    Vizkit.exec()

    Readline.readline()
    
    STDERR.puts "shutting down"
end
