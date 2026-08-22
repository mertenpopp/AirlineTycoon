$LOAD_PATH.unshift(File.dirname(__FILE__))
require 'support.rb'

require 'etc'
require 'thread'
require 'tsort'
require 'yaml'

def terminate
    $threads.each { |t| t.terminate }
    $watchdog.terminate
    $feeder.terminate
end

#Signal.trap("INT")  { terminate }
#Signal.trap("STOP") { terminate }
#Signal.trap("QUIT") { terminate }
#Signal.trap("HUP")  { terminate }

Struct.new("Job", :name, :cmd, :make_report, :depends, :enables, :quick, :isTest, :started)
Struct.new("Test", :name, :yaml, :status, :cycles, :operations, :pmem, :memory)

Struct.new("Message", :type, :name, :data)
def create_message(type, name=nil, data=nil)
    Struct::Message.new(type, name, data)
end

def compare_perf(val, ref)
    if ref == val
        return "\e[32mOK"
    else
        cmp = ref <=> val
        return "\e[#{32+cmp}m#{val}/#{ref}"
    end
end

class DepList < Hash
    include TSort
    alias tsort_each_node each_key
    def tsort_each_child(node, &block)
        fetch(node).each(&block)
    end
end

class ThreadPool
    def initialize
        @depList = DepList.new
        @waitList = Hash.new
        @testStatusList = Hash.new
    end

    def self.show_legend
        puts "Legend for job status:"
        puts "\e[37mSKIPPED \tJob was skipped by user."
        puts "\e[0mREADY    \tJob is ready for execution, but was not started yet."
        puts "\e[0mRUNNING  \tJob is currently running."
        puts "\e[33mWAITING \tJob cannot be started because a prerequisite did not finish yet."
        puts "\e[32mSUCCESS \tJob completed successfully."
        puts "\e[31mFAILURE \tJob has failed."
        puts "\e[31mBLOCKED \tJob is blocked because a prerequisite has failed."
        puts "\e[0m"
    end

    def add_job(name, cmd, file=nil)
        depends = Array.new
        quick = false
        isTest = true
        if file
            yaml = YAML.load_file(file)
            report = yaml.has_key?("Report")
            depends = yaml["Uses"].to_a if yaml["Uses"]
            quick = yaml["Quick"]
            isTest = yaml["IsTest"]
        else
            report = false
        end
        @depList[name] = depends
        @testStatusList[name] = Struct::Test.new(name, yaml, "\e[33mWAITING", [], [], [], [])
        job = Struct::Job.new(name, cmd, report, depends, [], quick, isTest, false)
        @waitList[name] = job
    end

    def check_dependencies
        @waitList.each_value do |job|
            if !job.depends.all? { |dep| @testStatusList.has_key?(dep) }
                puts "Unknown dependency for job #{job.name}"
                return false
            end
        end
        begin
            @depList.tsort
        rescue TSort::Cyclic
            puts "Found circular dependency!"
            return false
        end
        return true
    end

    def start(nrWorkers, showProgress, runQuickTest, noBuild)
        exit 1 unless check_dependencies

        # A job is either a test or a device/core/system (:= build task)
        # We execute a test if either
        # a.) we run a quick (smoke) test and the test is marked as quick test
        # b.) we run a full test
        # Here we collect the tests we want to execute:
        jobs = @waitList.select do |name, job|
            job.isTest && (job.quick || !runQuickTest)
        end

        # now we need to collect all build dependencies
        # we do this in a stabilization loop where we always iterate over all
        # jobs that were added in the previous iteration and add their
        # dependencies to newJobs
        # if noBuild was given, we skip all dependencies
        jobsToRun = Hash.new
        jobsToRun.update(jobs)
        if !noBuild
            loop do
                newJobs = Hash.new
                jobs.each do |name, job|
                    job.depends.each do |dependency|
                        if !jobsToRun.has_key?(dependency)
                            newJobs[dependency] = @waitList[dependency]
                        end
                    end
                end
                jobsToRun.update(newJobs)
                jobs = newJobs
                break if newJobs.empty?
            end
        end

        # set up message queues
        toWorker = Queue.new
        toWatchdog = Queue.new
        toFeeder = Queue.new

        @waitList.each do |name, job|
            next if jobsToRun.has_key?(name)
            # tell feeder that this job is already done
            # it needs to know since jobs may depend on this one
            @waitList[name].started = true
            toWatchdog << create_message(:SKIPPED, name)
        end

        @waitList.each_value do |job|
            # job.enables are dependencies in reverse direction
            job.depends.each do |dependencyName|
                @waitList[dependencyName].enables << job
            end
        end

        # @testStatusList is not thread-safe. This thread is the only one that may access it.
        $watchdog = Thread.new do
            watchdog_main(toWatchdog, toFeeder, @testStatusList, showProgress)
        end

        # @waitList is not thread-safe. This thread is the only one that may access it.
        $feeder = Thread.new do
            feeder_main(toFeeder, toWorker, toWatchdog, @waitList)
        end

        $threads = (0...nrWorkers).map do
            Thread.new do
                worker_main(toWorker, toWatchdog)
            end
        end

        # start feeder
        toFeeder << create_message(:START)
        # wait until workers are finished
        $threads.each { |t| t.join }
        # ask watchdog thread to terminate via message
        # this makes sure all previous messages were processed
        toWatchdog << create_message(:KILL)
        $watchdog.join

        return $watchdog.value
    end

    # function run by feeder thread
    def feeder_main(toFeeder, toWorker, toWatchdog, waitList)
        while msg = toFeeder.pop do # blocking
            jobname = ""
            # this array contains jobs that need to be checked
            # whether or not they are ready for execution
            # (which is the case if all dependencies are met) 
            enabledJobs = Array.new
            if msg.type == :FINISHED
                # a job has finished, check all jobs that depend on it
                jobname = msg.name
                next unless waitList.has_key?(jobname)
                enabledJobs = waitList[jobname].enables
                waitList.delete(jobname)
            elsif msg.type == :START
                # we've just started, check all jobs
                enabledJobs = waitList.values
            else
                next
            end

            enabledJobs.each do |enabledJob|
                if msg.type == :FINISHED
                    if msg.data
                        # prerequisite done, delete from dependency list
                        enabledJob.depends.delete(jobname) if jobname
                    else
                        # prerequisite has failed, job is blocked
                        toWatchdog << create_message(:BLOCKED, enabledJob.name)
                    end
                end
                if enabledJob.depends.empty? && !enabledJob.started
                    # submit job if no dependencies left
                    waitList[enabledJob.name].started = true
                    toWatchdog << create_message(:READY, enabledJob.name)
                    toWorker << enabledJob
                end
            end
            break if waitList.empty?
        end
        # ask worker threads to terminate via message
        # this makes sure all previous messages were processed
        toWorker << Struct::Job.new(:KILL, nil, nil, nil)
    end

    # function run by worker threads
    def worker_main(toWorker, toWatchdog)
        while job = toWorker.pop do # blocking
            break if job.name == :KILL
            toWatchdog << create_message(:START, job.name)
            ret = system(job.cmd)
            toWatchdog << create_message(:FINISHED, job.name, ret)
            next unless ret             # statistics are meaningless on failure
            next unless job.make_report # is job flagged for report generation?
            ret = _make_report(job.name)
            next unless ret             # not every regression has statistics
            toWatchdog << create_message(:REPORT, job.name, ret)
        end
        # keep kill message in queue (in order to kill all workers)
        toWorker << job
    end

    # function run by watchdog
    def watchdog_main(toWatchdog, toFeeder, statusList, showProgress)
        success = Array.new
        failures = Array.new
        indent = statusList.keys.map { |t| t.length() }.max()
        allNames = statusList.keys.sort
        allNames.each{ |n| puts n }

        while msg = toWatchdog.pop do # blocking
            case msg.type
            when :KILL
                break
            when :READY
                statusList[msg.name].status = "READY"
            when :START
                statusList[msg.name].status = "RUNNING"
            when :FINISHED
                if msg.data
                    statusList[msg.name].status = "\e[32mSUCCESS"
                    success << msg.name
                else
                    statusList[msg.name].status = "\e[31mFAILURE"
                    failures << msg.name
                end
                toFeeder << msg # forward message to feeder (start depending jobs)
            when :SKIPPED
                statusList[msg.name].status = "\e[37mSKIPPED"
                success << msg.name
                # user skipped job, notify as successfully finished to feeder
                toFeeder << create_message(:FINISHED, msg.name, true)
            when :BLOCKED
                statusList[msg.name].status = "\e[31mBLOCKED"
                failures << msg.name
                # job is blocked, notifiy feeder (depending jobs are also blocked)
                toFeeder << create_message(:FINISHED, msg.name, false)
            when :REPORT
                reference = statusList[msg.name].yaml["Report"]
                reference["cycles"].each do |proc, cycles|
                    report = msg.data["cycles"]
                    if report[proc]
                        statusList[msg.name].cycles << compare_perf(report[proc], cycles)
                    else
                        statusList[msg.name].cycles << "\e[31mN/A"
                    end
                end
                reference["operations"].each do |proc, operations|
                    report = msg.data["operations"]
                    if report[proc]
                        statusList[msg.name].operations << compare_perf(report[proc], operations)
                    else
                        statusList[msg.name].operations << "\e[31mN/A"
                    end
                end
                reference.each do |name, ref|
                    next if name == "cycles"
                    next if name == "operations"
                    report = msg.data[name]
                    if !report
                        statusList[msg.name].pmem << "\e[31mN/A"
                        statusList[msg.name].memory << "\e[31mN/A"
                    else
                        statusList[msg.name].pmem << compare_perf(report[0], ref["pmem usage"])
                        statusList[msg.name].memory << compare_perf(report[1], ref["memory usage"])
                    end
                end
                statusList[msg.name].status += "\t\e[0mPMEM: %s \e[0mDMEM: %s \e[0mCycles: %s \e[0mOps: %s" \
                    % [statusList[msg.name].pmem.join(", "),
                       statusList[msg.name].memory.join(", "),
                       statusList[msg.name].cycles.join(", "),
                       statusList[msg.name].operations.join(", ")]
            end
            next unless showProgress
            # delete last statusList.size lines
            print "\r" + ("\e[A\e[K" * statusList.size)
            # print status for each job
            allNames.each do |name|
                test = statusList[name]
                puts "%-#{indent}s\t#{test.status}\e[0m" % name
            end
        end
        puts "OK: #{success.size}"
        puts "Failures: #{failures.size}"
        failures.each do |name|
            puts "#{name} failed"
        end
        return failures.size == 0 && success.size == @testStatusList.size # be paranoid
    end
end

miss = (0..5).to_a()
miss += (11..20).to_a()
miss += (41..50).to_a()
name = "_mission_test"
miss.delete(19)

#miss = (0..5).to_a().map{|i|5*i}
#name = "_kMinimumOwnRouteUtilization"

#miss = (1..20).to_a()
#name = "kScheduling"

miss = [-1]
name = ""

#miss = [1, 12, 13, 15, 41]
#name = "_mission_new"

tp = ThreadPool.new
miss.map{|i| i}.each do |i|
  (0...300).each do |j|
    prefix = "dataMISS_#{i}#{name}_#{j}"
    prefix = "dataCLAUDE_freegame#{name}_#{j}" if i == -1
    file = "#{prefix}.csv"
    log = "#{prefix}.txt"

    gdb = ""
    #gdbcmd = "#{prefix}.gdb"
    #gdb = "gdb --command=#{gdbcmd} --args"
    #if !gdb.empty?
    #  File.open("#{gdbcmd}", "w") do |file|
    #    file.puts("set width 0")
    #    file.puts("set height 0")
    #    file.puts("set verbose off")
    #    file.puts("show args")
    #    file.puts("run /quick #{i}")
    #    file.puts("bt")
    #    file.puts("q")
    #  end
    #end

    tp.add_job("param=#{i}, run=#{j}", "#{gdb} ./AT /quick #{i} 2>&1 | tee #{log} | grep -E 'BotMission|BotStat' > #{file}") unless File.exist?(file)
    #tp.add_job("param=#{i}, run=#{j}", "#{gdb} ./AT /quick -1 /testbot #{i} 2>&1 | tee #{log} | grep -E 'BotMission|BotStat' > #{file}") unless File.exist?(file)
    end
end
# Worker count must not exceed the number of cores.
#
# The game seeds its flight-job pool from the wall clock -
# `qPlayer.Auftraege.Random.SRand(AtGetTime())` (Sim.cpp:809), and AtGetTime() is
# steady_clock + SDL_GetTicks64() (Misc.cpp:2177) - so a run's seed is decided by
# millisecond scheduling jitter. Oversubscribing the cores feeds that jitter a common
# machine-load signal, which correlates every game in a measurement and puts a floor under
# the noise that more games cannot lift.
#
# Measured on 24 cores, three 300-game measurements per pool size (sd of the reported score):
#   pool 100 -> 36.4M   pool 50 -> 31.9M   pool 24 -> 7.6M   pool 12 -> 5.8M
# and the runtime is identical from 24 upwards (~443s) because the work is CPU bound; pool 12
# costs 50% more for nothing. At pool 24 the spread is already at the pure-sampling floor
# (per-game sd 156M / sqrt(300) = 9.0M), i.e. the correlation is gone and the remaining noise
# is honest sampling that shrinks with the game count again.
tp.start([Etc.nprocessors, 24].min, false, false, false)
