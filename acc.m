function y = acc(A, K)
y = zeros(1,K);
sum = 0.0;
diff = 0.0;
last_k = 1;
for k = 1:K
    if A(6,k) >= 1.0 || A(6,k) <= -1.0
        if (sum >= 0 && sum + A(8,k) >= 0) || (sum < 0 && sum + A(8,k) < 0)
            sum = sum + A(8,k);
        else
            diff = diff + A(8,k);
        end
        if k > 1 && ((A(6,k-1) < 0 && A(6,k) >= 0) || (A(6,k-1) >= 0 && A(6,k) < 0))
            for i = last_k:k-1
                y(i) = y(i) - diff;
            end
            last_k = k;
            diff = 0;
            sum = A(8,k);
        end
        y(k) = sum;
    else
        if k > 1
            for i = last_k:k-1
                y(i) = y(i) - diff;
            end
            last_k = k;
        end
        diff = 0;
        sum = A(8,k);
        y(k) = 0.0;
    end
end
end
