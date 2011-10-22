local core = require "bignum.core"
module("bignum", package.seeall)

_COPYRIGHT = "Copyright (C) 2007  Rodrigo Cacilhas"
_DESCRIPTION = "Big number high-level support"
_NAME = "bignum"
_VERSION = "1.0"


--===================--
-- Metatabela BIGNUM --
--===================--


local bnmt = {}
bnmt.__index = bnmt


--===================--
-- Funções do módulo --
--===================--


--- Creates and returns a bignum object.
-- @param o A string, number or BIGNUM C userdata that represents a big integer.
-- @return A new bignum object.
function new(o)
	return bnmt:new(o)
end


--- Copies a bignum object to another.
-- @param to The destination bignum object.
-- @param from The source bignum object.
-- @return The BIGNUM C userdata from destination bignum object or nil.
function copy(to, from)
	assert(
		getmetatable(to) == bnmt and
		getmetatable(from) == bnmt,
		"parameters must be bignum"
	)
	return core.copy(to._BIGNUM, from._BIGNUM)
end


--- Swaps the values from two bignum objects.
-- @param a A bignum object.
-- @param b Another bignum object.
function swap(a, b)
	assert(
		getmetatable(a) == bnmt and
		getmetatable(b) == bnmt,
		"parameters must be bignum"
	)
	return core.swap(a._BIGNUM, b._BIGNUM)
end


--- Performs the addition of two big intengers, that can be under a modulo.
-- @param r The destination bignum object, where the result of the addition must
-- be stored.
-- @param a A bignum object, parameter of the addition.
-- @param b The other bignum object, parameter of the addition.
-- @param m The bignum object representing the modulo, if the result must be under
-- a modulo, or else nil.
-- @return True or false, if the addition succeeds or don't. 
function add(r, a, b, m)
	assert(
		getmetatable(r) == bnmt and
		getmetatable(a) == bnmt and
		getmetatable(b) == bnmt,
		"parameters must be bignum"
	)
	if m then
		assert(metatable(m) == bnmt, "parameters must be bignum")
		return core.mod_add(r._BIGNUM, a._BIGNUM, b._BIGNUM, m._BIGNUM)
	else
		return core.add(r._BIGNUM, a._BIGNUM, b._BIGNUM)
	end
end


--- Performs the subtraction of two big intengers, that can be under a modulo.
-- @param r The destination bignum object, where the result of the subtraction
-- must be stored.
-- @param a A bignum object, parameter of the subtraction.
-- @param b The other bignum object, parameter of the subtraction.
-- @param m The bignum object representing the modulo, if the result must be under
-- a modulo, or else nil.
-- @return True or false, if the subtraction succeeds or don't.
function sub(r, a, b, m)
	assert(
		getmetatable(r) == bnmt and
		getmetatable(a) == bnmt and
		getmetatable(b) == bnmt,
		"parameters must be bignum"
	)
	if m then
		assert(metatable(m) == bnmt, "parameters must be bignum")
		return core.mod_sub(r._BIGNUM, a._BIGNUM, b._BIGNUM, m._BIGNUM)
	else
		return core.sub(r._BIGNUM, a._BIGNUM, b._BIGNUM)
	end
end


--- Performs the multiplication of two big intengers, that can be under a modulo.
-- @param r The destination bignum object, where the product of the multiplication
-- must be stored.
-- @param a A bignum object, parameter of the multiplication.
-- @param b The other bignum object, parameter of the multiplication.
-- @param m The bignum object representing the modulo, if the product must be
-- under a modulo, or else nil.
-- @return True or false, if the multiplication succeeds or don't.
function mul(r, a, b, m)
	assert(
		getmetatable(r) == bnmt and
		getmetatable(a) == bnmt and
		getmetatable(b) == bnmt,
		"parameters must be bignum"
	)
	if m then
		assert(getmetatable(m) == bnmt, "parameters must be bignum")
		return core.mod_mul(r._BIGNUM, a._BIGNUM, b._BIGNUM, m._BIGNUM)
	else
		return core.mul(r._BIGNUM, a._BIGNUM, b._BIGNUM)
	end
end


--- Takes the square of a bignum object, that can be under a modulo.
-- @param r The destination bignum object, where the result must be stored.
-- @param a The source bignum object, the base.
-- @param m The bignum object representing the modulo, if the result must be under
-- a modulo, or else nil.
-- @return True or false, if the square succeeds or don't.
function sqr(r, a, m)
	assert(
		getmetatable(r) == bnmt and
		getmetatable(a) == bnmt,
		"parameters must be bignum"
	)
	if m then
		assert(metatable(m) == bnmt, "parameters must be bignum")
		return core.mod_sqr(r._BIGNUM, a._BIGNUM, m._BIGNUM)
	else
		return core.sqr(r._BIGNUM, a._BIGNUM)
	end
end


-- Raises a bignum object to a given power, and everything can be under a modulo.
-- @param r The destination bignum object, where the raising must be stored.
-- @param a The base bignum object.
-- @param p The power bignum object.
-- @param m The bignum object representing the modulo, if it must be under a
-- modulo, or else nil.
-- @return True or false, if the exponantiation succeeds or don't.
function exp(r, a, p, m)
	assert(
		getmetatable(r) == bnmt and
		getmetatable(a) == bnmt and
		getmetatable(p) == bnmt,
		"parameters must be bignum"
	)
	if m then
		assert(getmetatable(m) == bnmt, "parameters must be bignum")
		return core.mod_exp(r._BIGNUM, a._BIGNUM, p._BIGNUM, m._BIGNUM)
	else
		return core.exp(r._BIGNUM, a._BIGNUM, p._BIGNUM)
	end
end


--- Performs the integer division of two big integers.
-- @param dv The destination bignum object, where the division must be stored.
-- @param a The share bignum object.
-- @param d The divisorr bignum object.
-- @param rem A bignum object to store the remainder of the division, if you want.
-- @return True or false, if the division succeeds or don't.
function div(dv, a, d, rem)
	assert(
		getmetatable(dv) == bnmt and
		getmetatable(a) == bnmt and
		getmetatable(d) == bnmt,
		"parameters must be bignum"
	)
	if rem then
		assert(metatable(rem) == bnmt, "parameters must be bignum")
	end
	
	return core.div(dv._BIGNUM, rem and rem._BIGNUM, a._BIGNUM, d._BIGNUM)
end


--- Returns the representation of a big integer in a finite set, i.e., under a
-- modulo.
-- @param rem The destination bignum object, where the representation must be
-- stored.
-- @param a The source bignum object.
-- @param m The modulo.
-- @return True or false, if the operation succeeds or don't.
function mod(rem, a, m)
	assert(
		getmetatable(rem) == bnmt and
		getmetatable(a) == bnmt and
		getmetatable(m) == bnmt,
		"parameters must be bignum"
	)
	
	return core.nnmod(rem._BIGNUM, a._BIGNUM, m._BIGNUM)
end


--- Performs the discrete division of two big integers in a finite set of a prime
-- amount of elements.
-- @param dv The destination bignum object, where the division must be stored.
-- @param a The share bignum object.
-- @param d The divisor bignum object.
-- @param m The bignum object representing the modulo.
-- @return True or false, if the division succeeds or don't.
function moddiv(dv, a, d, m)
	assert(getmetatable(d) == bnmt, "parameters must be bignum")
	
	local b = d:inverse(m)
	local resp = mul(dv, a, b, m)
	b:close()
	return resp
end


--- Returns the greast common divisor of two big integers.
-- @param r The destination bignum object, where GCD must be stored.
-- @param a A bignum object.
-- @param b Another bignum object.
-- @return True or false, if the operation succeeds or don't.
function gcd(r, a, b)
	assert(
		getmetatable(r) == bnmt and
		getmetatable(a) == bnmt and
		getmetatable(b) == bnmt,
		"parameters must be bignum"
	)
	
	return core.gcd(r._BIGNUM, a._BIGNUM, b._BIGNUM)
end


--- Returns a bignum object with the special BIGNUM C userdata value 1.
-- @return The bignum object with the value 1.
function value_one()
	-- value_one não pode sofrer BN_free()
	local t = {
		_BIGNUM = core.value_one(),
		close = function ()
			error "BN_free() cannot run for value_one"
		end,
	}
	return setmetatable(t, bnmt)
end


--================--
-- Série BN_new() --
--================--


function bnmt:new(num)
	local _BIGNUM
	
	if type(num) == "number" then
		num = tostring(num)
	end
	
	if type(num) == "userdata" then
		_BIGNUM = num
	else
		_BIGNUM = core.new()
		core.init(_BIGNUM)
	end
	
	o = setmetatable({ _BIGNUM = _BIGNUM }, self)
	
	if type(num) == "string" then
		o:set(num)
	end
	
	return o
end


--- Frees the memory used by the BIGNUM C userdata.
function bnmt:__gc()
	print("gc'd bignum:", self)
	if self._BIGNUM then
		core.free(self._BIGNUM)
		self._BIGNUM = nil
	end
end


--=================--
-- Série BN_copy() --
--=================--


--- Reads the BIGNUM C userdata of another bignum object to the self one.
-- @param o Another bignum object.
function bnmt:copy(o)
	return copy(self, o)
end


--- Returns a copy of the self object.
-- @return A copy of the self object.
function bnmt:dup()
	return bnmt:new(core.dup(self._BIGNUM))
end


--- Swaps the BIGNUM C userdata with another bignum object.
-- @param o Another bignum object.
function bnmt:swap(o)
	return swap(self, o)
end


--======================--
-- Série BN_num_bytes() --
--======================--


--- Returns the number of bytes required to store the big integer.
-- @return Number of bytes.
function bnmt:bytes()
	return core.num_bytes(self._BIGNUM)
end


--- Returns the number of bits required to represent the big integer.
-- @return Number of bits.
function bnmt:bits()
	return core.num_bits(self._BIGNUM)
end


--=========================--
-- Série BN_set_negative() --
--=========================--


function bnmt:set_negative(n)
	assert(type(n) == "number", "parameter must be number")
	core.set_negative(self._BIGNUM, n)
end


--- Tells if the big integer is negative.
-- @return True of false, if the big integer is negative or not.
function bnmt:is_negative()
	return core.is_negative(self._BIGNUM)
end


--================--
-- Série BN_add() --
--================--


function bnmt:__add(o)
	local clear = false
	local r = bnmt:new()
	if type(o) == "number" then
		o = bnmt:new(o)
		clear = true
	end
	add(r, self, o)
	if clear then o:close() end
	return r
end


function bnmt:__sub(o)
	local clear = false
	local r = bnmt:new()
	if type(o) == "number" then
		o = bnmt:new(o)
		clear = true
	end
	sub(r, self, o)
	if clear then o:close() end
	return r
end


function bnmt:__mul(o)
	local clear = false
	local r = bnmt:new()
	if tonumber(o) == "2" then
		core.lshift1(r._BIGNUM, self._BIGNUM)
	else
		if type(o) == "number" then
			o = bnmt:new(o)
			clear = true
		end
		mul(r, self, o)
	end
	if clear then o:close() end
	return r
end


function bnmt:__div(o)
	local clear = false
	local r = bnmt:new()
	if tonumber(o) == "2" then
		core.rshift1(r._BIGNUM, self._BIGNUM)
	else
		if type(o) == "number" then
			o = bnmt:new(o)
			clear = true
		end
		div(r, self, o)
	end
	if clear then o:close() end
	return r
end


function bnmt:__pow(o)
	local clear = false
	local r = bnmt:new()
	if tonumber(o) == "2" then
		sqr(r, self)
	else
		if type(o) == "number" then
			o = bnmt:new(o)
			clear = true
		end
		exp(r, self, o)
	end
	if clear then o:close() end
	return r
end


function bnmt:__mod(o)
	local clear = false
	local r = bnmt:new()
	if type(o) == "number" then
		o = bnmt:new(o)
		clear = true
	end
	mod(r, self, o)
	if clear then o:close() end
	return r
end


--- Returns a new bignum object representing the square of the self one.
-- @return Another bignum object that's the square of the self one.
function bnmt:sqr(m)
	local r = bnmt:new()
	sqr(r, self, m)
	return r
end


--================--
-- Série BN_cmp() --
--================--


function bnmt:__eq(o)
	local clear = false
	local r
	if type(o) == "number" then
		o = bnmt:new(o)
		clear = true
	end
	r = core.cmp(self._BIGNUM, o._BIGNUM)
	if clear then o:close() end
	return r == 0
end


function bnmt:__le(o)
	local clear = false
	local r
	if type(o) == "number" then
		o = bnmt:new(o)
		clear = true
	end
	r = core.cmp(self._BIGNUM, o._BIGNUM)
	if clear then o:close() end
	return r < 1
end


function bnmt:__lt(o)
	local clear = false
	local r
	if type(o) == "number" then
		o = bnmt:new(o)
		clear = true
	end
	r = core.cmp(self._BIGNUM, o._BIGNUM)
	if clear then o:close() end
	return r == -1
end


--- Tells if the self object is zero.
-- @return True or false, if the self object is zero or not. 
function bnmt:is_zero()
	return core.is_zero(self._BIGNUM)
end


--- Tells if the self object is one.
-- @return True or false, if the self object is one or not.
function bnmt:is_one()
	return core.is_one(self._BIGNUM)
end


--- Tells if the self object is even.
-- @return True or false, if the self object is even or not.
function bnmt:is_even()
	return not core.is_odd(self._BIGNUM)
end


--- Tells if the self object is odd.
-- @return True or false, if the self object is odd or not.
function bnmt:is_odd()
	return core.is_odd(self._BIGNUM)
end


--=================--
-- Série BN_zero() --
--=================--


--- Sets the self object to zero.
-- @return True or false, if the performance succeeds or don't.
function bnmt:zero()
	return core.zero(self._BIGNUM)
end


--- Sets the self object to one.
-- @return True or false, if the performance succeeds or don't.
function bnmt:one()
	return core.one(self._BIGNUM)
end


--=================--
-- Série BN_rand() --
--=================--


--- Sets the self object to a random big integer with a determinated amount of
-- bits.
-- @param bits Length of bits of the big integer.
-- @param top Set the two most significant bits to 1 if true.
-- @param bottom Set odd if true.
-- @return True or false, if the performance succeeds or don't.
function bnmt:rand(bits, top, bottom)
	assert(type(bits) == "number", "bits must be number")
	top = top and true or false
	bottom = bottom and true or false
	return core.rand(self._BIGNUM, bits, top, bottom)
end


--- Sets the self object to a non-negative random big integer less than a limit.
-- @param o Another bignum object representing the roof limit.
-- @return True or false, if the performance succeeds or don't.
function bnmt:rand_range(o)
	assert(
		getmetatable(o) == getmetatable(self),
		"range must be bignum"
	)
	return core.rand_range(self._BIGNUM, o._BIGNUM)
end


--===========================--
-- Série BN_generate_prime() --
--===========================--


--- Sets the self object to a random prime.
-- @param bits Length of bits.
-- @param safe If it must be a safe prime, true or false.
-- @param add If it's not nil, the prime must fulfill the condition p % add == rem
-- (or p % add == 1 if rem is nil).
-- @param rem The remainder for the add param.
-- @return True or false, if the generation succeeds or don't.
function bnmt:generate_prime(bits, safe, add, rem)
	assert(type(bits) == "number", "bits must be number")
	safe = safe and true or false
	add = add and add._BIGNUM
	rem = rem and rem._BIGNUM
	return core.generate_prime(self._BIGNUM, bits, safe, add, rem) and true
end


--- Checks if the bignum object is prime.
-- @param checks Number of checking tests. Error probability: 0.25 ^ checks.
-- @return True or false, if the integer is prime or not.
function bnmt:is_prime(checks)
	-- Probabilidade de erro: 0.25 ^ checks
	checks = checks or 16
	return core.is_prime(self._BIGNUM, checks)
end


--- Checks if the bignum object is a safe prime.
-- @param checks Number of checking tests. Error probability: 0.25 ^ checks.
-- @return True or false, if the integer is a safe prime or not.
function bnmt:is_safe_prime(checks)
	if not self:is_prime() then return false end
	local r
	local aux = bnmt:new()
	local one = value_one()
	aux:copy(self)
	sub(aux, aux, one)
	core.rshift1(aux._BIGNUM, aux._BIGNUM)
	r = aux:is_prime()
	aux:close()
	return r
end


--====================--
-- Série BN_set_bit() --
--====================--


--- Sets a bit to one.
-- @param n Index of the bit to be set.
-- @return True or false, if the bit is set or not.
function bnmt:set_bit(n)
	assert(type(n) == "number", "index must be number")
	return core.set_bit(self._BIGNUM, n)
end


--- Sets a bit to zero.
-- @param n Index of the bit to be clear.
-- @return True or false, if the bit is clean set or not.
function bnmt:clear_bit(n)
	assert(type(n) == "number", "index must be number")
	return core.clear_bit(self._BIGNUM, n)
end


--- Tells if a bit is one or zero.
-- @param n Index of the bit.
-- @return True if it's one or false if zero.
function bnmt:is_bit_set(n)
	assert(type(n) == "number", "index must be number")
	return core.is_bit_set(self._BIGNUM, n)
end


--- Truncates the self object to a length of bits.
-- @param n Length of bits.
-- @return True or false, if it has succeed or not.
function bnmt:mask_bits(n)
	assert(type(n) == "number", "index must be number")
	return core.mask_bits(self._BIGNUM, n)
end


--- Shifts the self object left by a n bits.
-- @param n Amount of bits to shift.
-- @return True or false, if it has succeed or not.
function bnmt:lshift(n)
	if n then
		assert(type(n) == "number", "parameter must be number")
		return core.lshift(self._BIGNUM, self._BIGNUM, n)
	else
		return core.lshift1(self._BIGNUM, self._BIGNUM)
	end
end


--- Shifts the self object right by a n bits.
-- @param n Amount of bits to shift.
-- @return True or false, if it has succeed or not.
function bnmt:rshift(n)
	if n then
		assert(type(n) == "number", "parameter must be number")
		return core.rshift(self._BIGNUM, self._BIGNUM, n)
	else
		return core.rshift1(self._BIGNUM, self._BIGNUM)
	end
end


--===================--
-- Série BN_bn2bin() --
--===================--


--- Returns a decimal string representation of the self object. If a parameter is
-- taken, it sets the big integer, considering the paremeter as a decimal string
-- representation of the new value.
-- @param num The new value as decimal string or nil.
-- @return A decimal string representation of the self object.
function bnmt:dec(num)
	if num then
		if type(num) == "number" then
			num = tostring(num)
		end
		if type(num) ~= "string" or num:find "%D" then
			return
		end
		core.dec2bn(self._BIGNUM, num)
	end
	return core.bn2dec(self._BIGNUM)
end


--- Returns a hexadecimal string representation of the self object. If a parameter
-- is taken, it sets the big integer, considering the paremeter as a hexadecimal
-- string representation of the new value.
-- @param num The new value as hexadecimal string or nil.
-- @return A hexadecimal string representation of the self object.
function bnmt:hex(num)
	if num then
		if type(num) ~= "string" then
			return
		end
		if num:find "^0x" then
			num = num:sub(3)
		end
		num = num:upper()
		if num:find "%X" then
			return
		end
		core.hex2bn(self._BIGNUM, num)
	end
	return core.bn2hex(self._BIGNUM)
end


--- Gets a decimal or hexadecimal string representation of a big integer and sets
-- the self object to this one.
-- @param num Decimal or hexadecimal string.
-- @return True or false, if it succeeds or don't.
function bnmt:set(num)
	if type(num) == "number" then
		num = tostring(num)
	end
	
	if getmetatable(num) == getmetatable(self) then
		self:copy(num)
		return true
	elseif type(num) == "userdata" then
		core.copy(self._BIGNUM, num)
		return true
	elseif type(num) == "string" then
		-- TODO: dec ou hex?
		if num:find "^0x" or num:find "[A-Fa-f]" then
			return self:hex(num) and true or false
		else
			return self:dec(num) and true or false
		end
	else
		-- Que parâmetro?
		return false
	end
end


function bnmt:__tostring()
	return self:dec()
end


--========================--
-- Série BN_mod_inverse() --
--========================--


--- Return a new bignum object representing the discrete inverse element of the
-- self one in a finite set of m elements.
-- @param m A bignum object representing the modulo of the finite set.
-- @return A bignum object that's the inverse elemento of the self one.
function bnmt:inverse(m)
	local clear = false
	local r = bnmt:new()
	if type(m) == "number" then
		m = bnmt:new(m)
		clear = true
	end
	core.mod_inverse(r._BIGNUM, self._BIGNUM, m._BIGNUM)
	if clear then m:close() end
	return r
end


--=============--
-- Complemento --
--=============--


--- Increases the self object by adding it the value 1.
-- @param m The modulo or nil.
-- @return True or false if it succeeds or don't.
function bnmt:increment(m)
	if m then
		assert(
			getmetatable(m) == getmetatable(bnmt),
			"modulo must be bignum"
		)
	end
	
	return core.increment(self._BIGNUM, m)
end


--- Decreases the self object by subtracting it the value 1.
-- @param m The modulo or nil.
-- @return True or false if it succeeds or don't.
function bnmt:decrement(m)
	if m then
		assert(
			getmetatable(m) == getmetatable(bnmt),
			"modulo must be bignum"
		)
	end
	
	return core.decrement(self._BIGNUM, m)
end


--================--
-- Diffie-Hellman --
--================--


--- Returns a string representation of the big-endian signed two's complement
-- (btwoc) of the self object using the base64 set.
-- @return A base64 btwoc string.
function bnmt:btwoc()
	-- Retorna btwoc do BIGNUM em base64
	return core.b64btwoc(self._BIGNUM)
end


--- Turns the string from base64 to btwoc (big-endian signed two's complement),
-- then to big integer and sets the self objecto to it.
-- @param s A base64 btwoc string.
-- @return True of false, if it works or don't.
function bnmt:frombtwoc(s)
	-- Calcula BIGNUM a partir de uma representação base64 de um btwoc
	assert(
		type(s) == "string",
		"parameter must be a base64 string, received " .. type(s)
	)
	assert(
		#s % 4 == 0 and
		not s:find "[^A-Za-z0-9+/=]",
		"parameter must be a base64 string, received: " .. s
	)
	
	core.unb64btwoc(self._BIGNUM, s)
end


--- Generate a modulo-generator pair to a Diffie-Hellman calculus.
-- @param bits Length of bits.
-- @param generator The generator. If it's nil, the function will use 2 or 5.
-- @return A table with the keys: modulus - the modulo; generator - the generator,
-- close - a function to free the memory used by all bignum objects in the table.
function generateDH(bits, generator)
	local aux, p, g
	bits = bits or 1024
	generator = generator or (2 + math.random(0, 1) * 3)
	assert(
		type(bits) == "number" and
		type(generator) == "number",
		"bits must be number"
	)
	aux = bnmt:new(generator)
	p = aux:is_prime()
	aux:close()
	
	assert(p, "generator must be prime")
	
	p, g = core.generate_DH(bits, generator)
	
	if p then
		return {
			modulus = bnmt:new(p),
			generator = bnmt:new(g),
			close = function(self)
				local k, v
				for k, v in pairs(self) do
					if getmetatable(v) == bnmt then
						v:close()
					end
				end
			end,
		}
	end
end


--- Calculates the Diff-Hellman public key based on the modulo-generator pair and
-- private key taken.
-- @param dh Table with modulus and generator keys.
-- @param priv_key The private key. If nil, the function will choose a random
-- bignum less than modulo.
-- @return The public key.
function calculate_pub_key(dh, priv_key)
	assert(
		getmetatable(dh.modulus) == bnmt and
		getmetatable(dh.generator) == bnmt,
		"first parameter must be Diffie-Hellman table"
	)
	if not priv_key then
		priv_key = bnmt:new()
		priv_key:rand_range(dh.modulus)
	end
	assert(getmetatable(priv_key) == bnmt, "keys must be bignum")
	local pub_key = bnmt:new()
	
	local resp = exp(pub_key, dh.generator, priv_key, dh.modulus)
	
	dh.priv_key = priv_key
	dh.pub_key = pub_key
	
	return resp
end




--=================--
-- yue rpc support --
--=================--
function __pack(bn, wb)
	core.pack(bn._BIGNUM, wb)
	return 'bignum'
end
function bnmt:__pack(wb) 
	return bignum.__pack(self, wb)
end

function __unpack(rb)
	print('unpack called')
	local r = bignum.new(core.unpack(rb));
	print('result bn:', r)
	return r
end

