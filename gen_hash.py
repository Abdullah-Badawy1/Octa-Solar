import bcrypt

password = 'testpass'
hashed = bcrypt.hashpw(password.encode('utf-8'), bcrypt.gensalt())
print(hashed)  # Print directly, no .decode() needed
