SELECT * FROM books WHERE id = 1;
SELECT * FROM books WHERE id BETWEEN 2 AND 4;
SELECT title,author FROM books WHERE author = 'George Orwell';
SELECT id,title,genre FROM books WHERE genre = 'SE';
INSERT INTO books VALUES ('The Pragmatic Programmer','Andrew Hunt','SE');
SELECT * FROM books WHERE author = 'Andrew Hunt';

