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
			end,
			secret_data = 'dr56yhu89k2',
			h = function(self, num)
				assert(num == 200)
				return self.secret_data .. num
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
