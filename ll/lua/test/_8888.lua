x = {
	y = {
		z = {
			f = function (num)
				assert(num == 100)
				return 'abcf' .. num
			end,
			g = function (num)
				assert(num == 100)
				return 'abcg' .. num
			end
		},
		w = function(num)
			assert(num == 100)
			return 'abg' .. num
		end
	},
	_y = {
		z = {
			f = function (num)
				assert(num == 100)
				return 'abcf' .. num
			end
		}
	},
}
