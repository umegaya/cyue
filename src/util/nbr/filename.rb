$stdin.each_line do |line|
	$stdout.write(File.basename(line))
end
