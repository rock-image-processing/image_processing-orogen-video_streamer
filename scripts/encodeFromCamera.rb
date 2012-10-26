#! /usr/bin/env ruby
# -*- coding: utf-8 -*-

require 'orocos'
require 'readline'
include Orocos
Orocos::CORBA::max_message_size = 100000000
Orocos.initialize

if !ARGV[0]  then 
    puts "usage: cameraTaskName"
    exit
end

camera = TaskContext.get ARGV[0]
if(!camera) 
then
    puts "Could not get camera task"
    exit
end

# setup the environment so that ruby can find the test deployment
ENV['PKG_CONFIG_PATH'] = "#{File.expand_path("..", File.dirname(__FILE__))}/build:#{ENV['PKG_CONFIG_PATH']}"

Orocos.run('video_streamer::VideoEncoder' => 'encoder', :valgrind => false) do
    encoder = TaskContext.get('encoder')
    camera.frame.connect_to( encoder.raw_pictures, :type => :buffer, :size => 30 );
    encoder.bitrate = 1024*1024*8
    
    encoder.configure()
    encoder.start()
    
    Readline.readline()
    
    STDERR.puts "shutting down"
end
