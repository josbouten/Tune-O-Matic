#!/usr/bin/env ruby
=begin
    frequency ranges for Tune-O-Matic firmware (Tuner)
    --------------------------------------------------

    Copy/paste the output in array named "frequencyTable"

    David Haillant, 20240212
=end

=begin
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
=end




require 'optparse'
options = {}
OptionParser.new do |opt|
	opt.on('-f', '--frequency <frequency>',	'A4 reference frequency (in Hz). Default is 440 Hz')		{ |f| options[:A4_reference] = f }
	opt.on('-c', '--cents <variation>',	'Variation allowed, in cents. Default is 10 cents.')		{ |c| options[:cents] = c }

	opt.on("-h", "--help", "Prints this help.") do
		puts opt
		exit
	end
end.parse!

# A4 reference frequency
if options[:A4_reference].to_i > 400 and options[:A4_reference].to_i < 500 then
	A4_reference = options[:A4_reference].to_f * 10
else
	A4_reference = 4400.0
end

if options[:cents].to_i > 0 and options[:cents].to_i < 50 then
	cents = options[:cents].to_f
else
	cents = 10.0
end




# we store the various frequency ranges in an array of structs
Frequency = Struct.new(:min_boundary, :min_acceptable, :central, :max_acceptable, :max_boundary)
frequencies = []

# we calculate the frequencies for 6 octaves + 1 note, from C0 to C6
for note in -57..15 do
  central_freq = (A4_reference * 2 ** (note / 12.0)).round(1)

  min_boundary   = (central_freq * 2 ** (-50.0 / 1200)).round(1).to_i
  min_acceptable = (central_freq * 2 ** (-cents / 1200)).round(1).to_i
  max_acceptable = (central_freq * 2 ** (cents / 1200)).round(1).to_i
  max_boundary   = (central_freq * 2 ** (50.0 / 1200)).round(1).to_i

  frequencies << Frequency.new(min_boundary, min_acceptable, central_freq, max_acceptable, max_boundary)
end


puts "Frequency ranges for Tune-O-Matic, in 1/10 Hz\n\n"
puts "Copy/paste output in array 'frequencyTable'\n\n"


puts "---------------------"
#frequencies.each do |f|
#  puts "#{f.min_boundary},\t#{f.min_acceptable},\t"
#  puts "#{f.min_acceptable},\t#{f.max_acceptable},\t"
#  puts "#{f.max_acceptable},\t#{f.max_boundary},\t"
#end

puts "  // A4 = #{A4_reference.to_i / 10} Hz"
puts "  // Allowed range = #{cents} cents\n\n"


note_names = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]

# i is note from 0 to 11
# j is octave from 0 to 6
for i in 0..11 do
  print "  "
  for j in 0..6 do
    if frequencies[i + 12 * j].nil?
      print "0,\t0,\t"
    else
      print "#{frequencies[i + 12 * j].min_boundary},\t#{frequencies[i + 12 * j].min_acceptable},\t"
    end
  end

  print "// #{note_names[i]}\n"


  print "  "
  for j in 0..6 do
    if frequencies[i + 12 * j].nil?
      print "0,\t0,\t"
    else
      print "#{frequencies[i + 12 * j].min_acceptable},\t#{frequencies[i + 12 * j].max_acceptable},\t"
    end
  end
  print "\n"


  print "  "
  for j in 0..6 do
    if frequencies[i + 12 * j].nil?
      print "0,\t0"
 
      if (i == 11) then
        print "\t"
      else
        print ",\t"
      end
    else
     print "#{frequencies[i + 12 * j].max_acceptable},\t#{frequencies[i + 12 * j].max_boundary},\t"
    end
  end
  print "\n"
end
print "\n"

